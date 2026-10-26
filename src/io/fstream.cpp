
#include <fstream>
#include <sstream>

#include <fc/filesystem.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/fstream.hpp>
#include <fc/log/logger.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fc {
   class ofstream::impl : public fc::retainable {
      public:
         boost::filesystem::ofstream ofs;
   };
   class ifstream::impl : public fc::retainable {
      public:
         boost::filesystem::ifstream ifs;
   };

   ofstream::ofstream()
   :my( new impl() ){}

   ofstream::ofstream( const fc::path& file, int m )
   :my( new impl() ) { this->open( file, m ); }
   ofstream::~ofstream(){}

   void ofstream::open( const fc::path& file, int m ) {
     const boost::filesystem::path& bfp = file;
     /* m — это fc::ofstream::mode, а НЕ std::ios_base::openmode. Числа у них совпадают
      * СЛУЧАЙНО (std::ios_base::app == 1 == fc::ofstream::binary), поэтому приведение
      * «как есть» превращало любое открытие ПО УМОЛЧАНИЮ (m = binary) в дозапись, и
      * json::save_to_file / aes_save / mmap_struct / кошелёк дописывали второй документ
      * в конец первого. Переводим флаги по одному; trunc ставим ЯВНО — обрезка была
      * историческим поведением open() и остаётся поведением всех, кто не просил append. */
     std::ios_base::openmode mode = std::ios_base::out;
     if( m & ofstream::binary ) mode |= std::ios_base::binary;
     if( m & ofstream::append ) mode |= std::ios_base::app;
     else                       mode |= std::ios_base::trunc;
     /* Сброс состояния: после неудачного открытия failbit липнет, и следующая попытка
      * (file_appender повторяет её каждую секунду) молча «открывала» бы тот же сбой. */
     my->ofs.clear();
     my->ofs.open( bfp, mode );
   }

   /* Отдельный геттер, а не исключение в open(): на fc::ofstream пишут и другие места
    * (json/снапшоты/кошелёк), и менять им поведение при сбое открытия — не наше дело.
    * Тот, для кого молчание смерти подобно (логгер), проверяет это сам. */
   bool ofstream::is_open() const {
     return my->ofs.is_open();
   }
   bool ofstream::good() const {
     return my->ofs.good();
   }
   void ofstream::clear() {
     my->ofs.clear();
   }
   size_t ofstream::writesome( const char* buf, size_t len ) {
        my->ofs.write(buf,len);
        return len;
   }
   size_t ofstream::writesome(const std::shared_ptr<const char>& buffer, size_t len, size_t offset)
   {
     return writesome(buffer.get() + offset, len);
   }

   void   ofstream::put( char c ) {
        my->ofs.put(c);
   }
   void   ofstream::close() {
        my->ofs.close();
   }
   void   ofstream::flush() {
        my->ofs.flush();
   }

   ifstream::ifstream()
   :my(new impl()){}
   ifstream::ifstream( const fc::path& file, int m )
   :my(new impl())
   {
      this->open( file, m );
   }
   ifstream::~ifstream(){}

   void ifstream::open( const fc::path& file, int m ) {
     const boost::filesystem::path& bfp = file; 
      my->ifs.open( bfp, std::ios::binary );
   }
   size_t ifstream::readsome( char* buf, size_t len ) {
      auto s = size_t(my->ifs.readsome( buf, len ));
      if( s <= 0 ) {
         read( buf, 1 );
         s = 1;
      }
      return s;
   }
   size_t ifstream::readsome(const std::shared_ptr<char>& buffer, size_t max, size_t offset)
   {
     return readsome(buffer.get() + offset, max);
   }

   ifstream& ifstream::read( char* buf, size_t len ) {
      if (eof())
        FC_THROW_EXCEPTION( eof_exception , "");
      my->ifs.read(buf,len);
      if (my->ifs.gcount() < int64_t(len))
        FC_THROW_EXCEPTION( eof_exception , "");
      return *this;
   }
   ifstream& ifstream::seekg( size_t p, seekdir d ) {
      switch( d ) {
        case beg: my->ifs.seekg( p, std::ios_base::beg ); return *this;
        case cur: my->ifs.seekg( p, std::ios_base::cur ); return *this;
        case end: my->ifs.seekg( p, std::ios_base::end ); return *this;
      }
      return *this;
   }
   void   ifstream::close() { return my->ifs.close(); }

   bool   ifstream::eof()const { return !my->ifs.good(); }

   void read_file_contents( const fc::path& filename, std::string& result )
   {
      const boost::filesystem::path& bfp = filename;
      boost::filesystem::ifstream f( bfp, std::ios::in | std::ios::binary );
      // don't use fc::stringstream here as we need something with override for << rdbuf()
      std::stringstream ss;
      ss << f.rdbuf();
      result = ss.str();
   }
  
} // namespace fc 
