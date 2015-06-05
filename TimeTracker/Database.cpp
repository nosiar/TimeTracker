#include "stdafx.h"
#include "Database.h"

using namespace std;

Database::Database() : is_open{ false }, db{ nullptr }
{

}

int Database::open(const char* file_name)
{
    auto r = sqlite3_open(file_name, &db);
    if (r == SQLITE_OK)
    {
        is_open = true;
    }
    return r == SQLITE_OK;
}

void Database::close()
{
    if (is_open)
        sqlite3_close(db);
}

static int callback(void *, int argc, char**argv, char**azColName)
{
    for (int i = 0; i < argc; ++i)
    {
        auto x = argv[i];
        auto y = azColName[i];
        UNREFERENCED_PARAMETER(x);
        UNREFERENCED_PARAMETER(y);
    }
    return 0;
}

int Database::create_table()
{
    char* err = nullptr;
    char* sql = "CREATE TABLE PROGRAM("  \
        "ID INTEGER PRIMARY KEY      AUTOINCREMENT," \
        "NAME           TEXT         NOT NULL," \
        "TIME           DATETIME     NOT NULL," \
        "DURATION       INT          NOT NULL);";
    return sqlite3_exec(db, sql, callback, 0, &err);
}

int Database::insert(char* name, char* datetime, int duration)
{
    char* err = nullptr;
    char sql[1024];
    sprintf_s(sql, "INSERT INTO PROGRAM (NAME,TIME,DURATION) "  \
                 "VALUES ('%s', '%s', %d );", name, datetime, duration);
    return sqlite3_exec(db, sql, callback, 0, &err);
}