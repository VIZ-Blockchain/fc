#include <fc/exception/exception.hpp>
#include <fc/io/fstream.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/thread/thread.hpp>
#include <fc/variant.hpp>
#include <boost/thread/mutex.hpp>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <queue>
#include <sstream>
#include <iostream>
#include <thread>

namespace fc {

   class file_appender::impl : public fc::retainable
   {
      public:
         config                     cfg;
         ofstream                   out;
         boost::mutex               slock;

      private:
         /* Ротация обслуживается СВОИМ потоком (vizd, Q#3072=A). Раньше цепочка жила в очереди
          * файберов fc::thread::current(), а vizd создаёт аппендер на главном потоке и уходит в
          * io_context::run() — очередь никто не крутит, и после стартовой ротации следующая задача
          * лежала вечно (замер: knowledge/log-rotation-never-fires.md). std::thread не зависит от
          * того, кто и как обслуживает fc-очереди. */
         std::thread                _timer_thread;
         std::atomic<bool>          _stop_timer{ false };
         time_point_sec             _current_file_start_time;
         fc::path                   _current_log_filename;   // файл, который сейчас открыт на запись
         bool                       _rotation_failed = false; // чтобы ошибка ротации не спамила каждую секунду

         time_point_sec get_file_start_time( const time_point_sec& timestamp, const microseconds& interval )
         {
             int64_t interval_seconds = interval.to_seconds();
             int64_t file_number = timestamp.sec_since_epoch() / interval_seconds;
             return time_point_sec( (uint32_t)(file_number * interval_seconds) );
         }

      public:
         impl( const config& c) : cfg( c )
         {
             if( cfg.rotate )
             {
                 FC_ASSERT( cfg.rotation_interval >= seconds( 1 ) );
                 FC_ASSERT( cfg.rotation_limit >= cfg.rotation_interval );
             }
         }

         ~impl()
         {
            _stop_timer.store( true );
            if( _timer_thread.joinable() )
                _timer_thread.join();
         }

         /* Стартовая ротация — синхронно, чтобы первый же лог шёл в часовой файл, и только потом
          * таймер: у file_appender::file_appender() каталог создаётся ПОСЛЕ конструктора impl. */
         void start()
         {
            /* Если стартовая ротация не удалась (каталог/путь недоступны), поток всё равно
             * поднимаем: таймер повторит попытку и восстановится, а не «замолчит навсегда». */
            try { rotate_files( true ); }
            catch( const fc::exception& e )
            {
                std::cerr << "file_appender: initial rotation failed: " << e.to_string() << "\n";
            }
            catch( const std::exception& e )
            {
                std::cerr << "file_appender: initial rotation failed: " << e.what() << "\n";
            }
            _timer_thread = std::thread( [this]() { timer_loop(); } );
         }

         void rotate_files( bool initializing = false )
         {
             FC_ASSERT( cfg.rotate );
             fc::time_point now = time_point::now();
             fc::time_point_sec start_time = get_file_start_time( now, cfg.rotation_interval );
             std::string timestamp_string = start_time.to_non_delimited_iso_string();
             fc::path link_filename = cfg.filename;
             fc::path log_filename = link_filename.parent_path() / (link_filename.filename().string() + "." + timestamp_string);

             {
               fc::scoped_lock<boost::mutex> lock( slock );

               if( !initializing )
               {
                   if( start_time <= _current_file_start_time )
                       return;   // ещё тот же час: таймер разбудит на следующей границе

                   out.flush();
                   out.close();
               }
               remove_all(link_filename);  // on windows, you can't delete the link while the underlying file is opened for writing
               /* Открытие может НЕ удаться (диск полон, каталог отобран, путь занят каталогом).
                * Раньше это проходило молча и аппендер замолкал до перезапуска, — поэтому при сбое
                * возвращаем на запись ПРЕЖНИЙ файл и сообщаем наверх через исключение. */
               try
               {
                   out.open( log_filename, std::ios_base::out | std::ios_base::app );
                   FC_ASSERT( out.is_open(), "cannot open log file ${f}", ("f", log_filename.preferred_string()) );
               }
               catch( ... )
               {
                   /* Возвращаем аппендер к жизни: пишем в ПРЕЖНИЙ файл и восстанавливаем на него
                    * ссылку — иначе `tail -f node.log` останется висеть на удалённом inode. */
                   if( !initializing && _current_file_start_time.sec_since_epoch() > 0 )
                   {
                       try { out.open( _current_log_filename, std::ios_base::out | std::ios_base::app ); }
                       catch( ... ) {}
                       try { create_hard_link( _current_log_filename, link_filename ); }
                       catch( ... ) {}
                   }
                   throw;
               }

               create_hard_link(log_filename, link_filename);
               _current_log_filename = log_filename;
               _current_file_start_time = start_time;
             }

             /* Delete old log files */
             fc::time_point limit_time = now - cfg.rotation_limit;
             std::string link_filename_string = link_filename.filename().string();
             directory_iterator itr(link_filename.parent_path());
             for( ; itr != directory_iterator(); itr++ )
             {
                 try
                 {
                     std::string current_filename = itr->filename().string();
                     if (current_filename.compare(0, link_filename_string.size(), link_filename_string) != 0 ||
                         current_filename.size() <= link_filename_string.size() + 1)
                       continue;
                     std::string current_timestamp_str = current_filename.substr(link_filename_string.size() + 1,
                                                                            timestamp_string.size());
                     fc::time_point_sec current_timestamp = fc::time_point_sec::from_iso_string( current_timestamp_str );
                     if( current_timestamp < start_time )
                     {
                         if( current_timestamp < limit_time || file_size( current_filename ) <= 0 )
                         {
                             remove_all( *itr );
                             continue;
                         }
                     }
                 }
                 catch (const fc::canceled_exception&)
                 {
                     throw;
                 }
                 catch( ... )
                 {
                 }
             }

         }

         /* Ждём до ближайшей границы часового файла и ротируем. Спим не дольше секунды, чтобы
          * остановка не ждала весь интервал, а перевод часов вперёд подхватывался сразу.
          * Исключение здесь не повод убивать процесс (std::thread → std::terminate) и не повод
          * крутиться в горячем цикле: ждём секунду и пробуем снова, как раньше молчал future. */
         void timer_loop()
         {
             while( !_stop_timer.load() )
             {
                 int64_t wait_us = 1000000;
                 try
                 {
                     fc::time_point_sec next;
                     {
                         fc::scoped_lock<boost::mutex> lock( slock );
                         next = _current_file_start_time + (uint32_t)cfg.rotation_interval.to_seconds();
                     }
                     fc::time_point now = time_point::now();
                     wait_us = (int64_t)next.sec_since_epoch() * 1000000 - now.time_since_epoch().count();
                     if( wait_us <= 0 )
                     {
                         rotate_files();
                         if( _rotation_failed )
                         {
                             _rotation_failed = false;
                             std::cerr << "file_appender: rotation recovered\n";
                         }
                         continue;
                     }
                 }
                 catch( const fc::exception& e )
                 {
                     wait_us = 1000000;
                     if( !_rotation_failed )
                     {
                         _rotation_failed = true;
                         std::cerr << "file_appender: rotation failed: " << e.to_string()
                                   << " — повтор через 1 с" << "\n";
                     }
                 }
                 catch( ... )
                 {
                     wait_us = 1000000;
                     if( !_rotation_failed )
                     {
                         _rotation_failed = true;
                         std::cerr << "file_appender: rotation failed (unknown error) — повтор через 1 с\n";
                     }
                 }
                 std::this_thread::sleep_for( std::chrono::microseconds( wait_us < 1000000 ? wait_us : 1000000 ) );
             }
         }
   };

   file_appender::config::config(const fc::path& p) :
     format( "${timestamp} ${thread_name} ${context} ${file}:${line} ${method} ${level}]  ${message}" ),
     filename(p),
     flush(true),
     rotate(false)
   {}

   file_appender::file_appender( const variant& args ) :
     my( new impl( args.as<config>() ) )
   {
      try
      {
         fc::create_directories(my->cfg.filename.parent_path());

         if(!my->cfg.rotate)
         {
            my->out.open( my->cfg.filename, std::ios_base::out | std::ios_base::app);
            FC_ASSERT( my->out.is_open(), "cannot open log file ${f}", ("f", my->cfg.filename.preferred_string()) );
         }
         else
            my->start();

      }
      catch( ... )
      {
         std::cerr << "error opening log file: " << my->cfg.filename.preferred_string() << "\n";
      }
   }

   file_appender::~file_appender(){}

   // MS THREAD METHOD  MESSAGE \t\t\t File:Line
   void file_appender::log( const log_message& m )
   {
      std::stringstream line;
      {
         int64_t us = m.get_context().get_timestamp().time_since_epoch().count();
         int ms     = (int)((us / 1000) % 1000);
         int64_t s  = us / 1000000;
         line << std::setfill('0')
              << std::setw(2) << (int)((s / 3600) % 24) << ':'
              << std::setw(2) << (int)((s / 60) % 60)   << ':'
              << std::setw(2) << (int)(s % 60)           << '.'
              << std::setw(3) << ms
              << std::setfill(' ') << ' ';
      }
      line << std::setw( 21 ) << (m.get_context().get_thread_name().substr(0,9) + std::string(":") + m.get_context().get_task_name()).c_str() << " ";

      std::string method_name = m.get_context().get_method();
      // strip all leading scopes...
      if( method_name.size() )
      {
         uint32_t p = 0;
         for( uint32_t i = 0;i < method_name.size(); ++i )
         {
             if( method_name[i] == ':' ) p = i;
         }

         if( method_name[p] == ':' )
           ++p;
         line << std::setw( 20 ) << m.get_context().get_method().substr(p,20).c_str() <<" ";
      }

      line << "] ";
      std::string message = fc::format_string( m.get_format(), m.get_data() );
      line << message.c_str();

      //fc::variant lmsg(m);

      // std::string fmt_str = fc::format_string( my->cfg.format, mutable_variant_object(m.get_context())( "message", message)  );

      {
        fc::scoped_lock<boost::mutex> lock( my->slock );
        my->out << line.str() << "\t\t\t" << m.get_context().get_file() << ":" << m.get_context().get_line_number() << "\n";
        if( my->cfg.flush )
          my->out.flush();
      }
   }

} // fc
