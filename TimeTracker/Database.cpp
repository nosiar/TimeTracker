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
    db->reader.size++;
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
        "TIME           DATETIME     NOT NULL," \
        "DURATION       INT          NOT NULL);";
    exec(sql);
}

void Database::insert(const char* name, const char* datetime, int duration)
{
    char sql[1024];
    sprintf_s(sql, "INSERT INTO PROGRAM (NAME,TIME,DURATION) "  \
        "VALUES ('%s', '%s', %d );", name, datetime, duration);
    exec(sql);
}

const Reader& Database::select(const char* name)
{
    char sql[1024];
    sprintf_s(sql, "SELECT * FROM PROGRAM WHERE NAME = '%s';", name);
    exec(sql);
    return reader;
}

void Reader::clear()
{
    reader.clear();
    size = 0;
    cur = -1;
}

bool Reader::read() const
{
    cur++;
    return cur < size;
}

std::string Reader::get(const char* column) const
{
    auto& v = reader.at(column);
    return v[cur];
}
