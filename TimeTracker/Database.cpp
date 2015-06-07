#include "stdafx.h"
#include "Database.h"

using namespace std;

Database::Database() : is_open{ false }, sqlite_db{ nullptr }, last_error{ 0 }, last_error_msg{ nullptr }
{

}

void Database::open()
{
    last_error = sqlite3_open("data.sqlite", &sqlite_db);
    if (last_error == SQLITE_OK)
        is_open = true;
}

void Database::close()
{
    if (is_open)
        sqlite3_close(sqlite_db);
}

int Database::callback(void *arg, int argc, char**argv, char**azColName)
{
    Database* db = (Database*)arg;
    db->reader.size_++;
    for (int i = 0; i < argc; ++i)
    {
        auto& v = db->reader.reader[azColName[i]];
        v.push_back(argv[i]);
    }
    return 0;
}

void Database::exec(const char* sql)
{
    reader.clear();

    if (last_error_msg != nullptr)
        sqlite3_free(last_error_msg);

    last_error = sqlite3_exec(sqlite_db, sql, callback, this, &last_error_msg);
}

void Database::create_table()
{
    char* sql = "CREATE TABLE PROGRAM("  \
        "ID INTEGER PRIMARY KEY      AUTOINCREMENT," \
        "NAME           TEXT         NOT NULL," \
        "SUBNAME        TEXT," \
        "TIME           DATETIME     NOT NULL," \
        "DURATION       INT          NOT NULL);";
    exec(sql);
}

void Database::insert(const char* name, const char* subname, const char* datetime, int duration)
{
    char sql[1024];
    sprintf_s(sql, "INSERT INTO PROGRAM (NAME,SUBNAME,TIME,DURATION) "  \
        "VALUES ('%s', '%s', '%s', %d );", name, subname, datetime, duration);
    exec(sql);
}

void Database::update(const char* name, const char* subname, const char* datetime, int new_duration)
{
    char sql[1024];
    sprintf_s(sql, "UPDATE PROGRAM set DURATION = %d "  \
        "WHERE NAME = '%s' AND SUBNAME = '%s' AND TIME = '%s'; ", new_duration, name, subname, datetime);
    exec(sql);
}

const Reader& Database::select(const char* name, const char* subname, const char* time)
{
    char sql[1024];
    sprintf_s(sql, "SELECT * FROM PROGRAM WHERE NAME = '%s' AND SUBNAME = '%s' AND TIME = '%s';", name, subname, time);
    exec(sql);
    return reader;
}

void Reader::clear()
{
    reader.clear();
    size_ = 0;
    cur_ = -1;
}

bool Reader::read() const
{
    cur_++;
    return cur_ < size_;
}

std::string Reader::get(const char* column) const
{
    auto& v = reader.at(column);
    return v[cur_];
}
