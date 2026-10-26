#pragma once

#include <fc/shared_ptr.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/iostream.hpp>

namespace fc {
    class path;

    class ofstream : virtual public ostream {
    public:
        /* Режим — СОБСТВЕННЫЙ enum fc, а НЕ std::ios_base. Значения совпадают СЛУЧАЙНО
         * (std::ios_base::app == 1 == fc::ofstream::binary), поэтому передавать их в
         * std::ofstream::open как есть нельзя: binary (1) стал бы app, и запись всех, кто
         * открывает файл «заново», молча перешла бы с обрезки на дозапись (json, кошелёк,
         * снапшоты). Перевод режима живёт в src/io/fstream.cpp::open.
         *   out    (0) — создать/обрезать (историческое поведение по умолчанию);
         *   binary (1) — двоичный режим; вместе с out тоже обрезает;
         *   append (2) — ДОПИСЫВАТЬ; нужен file_appender, который переоткрывает свой
         *                часовой файл после рестарта в том же часу. */
        enum mode {
            out = 0, binary = 1, append = 2
        };

        ofstream();

        ofstream(const fc::path &file, int m = binary);

        ~ofstream();

        void open(const fc::path &file, int m = binary);
        bool is_open() const;

        /* Состояние потока и его сброс — для тех, кому молчание смерти подобно (логгер: строка,
         * не попавшая в файл из-за полного диска, обязана быть замечена, а не проглочена badbit). */
        bool good() const;
        void clear();

        size_t writesome(const char *buf, size_t len);

        size_t writesome(const std::shared_ptr<const char> &buffer, size_t len, size_t offset);

        void put(char c);

        void close();

        void flush();

    private:
        class impl;

        fc::shared_ptr<impl> my;
    };

    class ifstream : virtual public istream {
    public:
        enum mode {
            in, binary
        };
        enum seekdir {
            beg, cur, end
        };

        ifstream();

        ifstream(const fc::path &file, int m = binary);

        ~ifstream();

        void open(const fc::path &file, int m);

        size_t readsome(char *buf, size_t len);

        size_t readsome(const std::shared_ptr<char> &buffer, size_t max, size_t offset);

        ifstream &read(char *buf, size_t len);

        ifstream &seekg(size_t p, seekdir d = beg);

        using istream::get;

        void get(char &c) {
            read(&c, 1);
        }

        void close();

        bool eof() const;

    private:
        class impl;

        fc::shared_ptr<impl> my;
    };

    /**
     * Grab the full contents of a file into a std::string object.
     * NB reading a full file into memory is a poor choice
     * if the file may be very large.
     */
    void read_file_contents(const fc::path &filename, std::string &result);

} // namespace fc
