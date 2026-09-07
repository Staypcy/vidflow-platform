#include "db.h"
#include<stdexcept>
#include<ctime>


static void check(int rc,sqlite3* db){
    if(rc!=SQLITE_OK && rc!=SQLITE_DONE && rc!= SQLITE_ROW){
        throw std::runtime_error(std::string("SQLite failed.") + sqlite3_errmsg(db));
    }
}

static sqlite3_stmt* prepare(sqlite3* db , const std::string& sql){
    sqlite3_stmt* stmt=nullptr;
    check(sqlite3_prepare_v2(db,sql.c_str(),-1,&stmt,nullptr),db);
}

static std::string now()noexcept{
    return std::to_string(std::time(nullptr));
}

DB::DB(const std::string &path)
{
    //开启SQLite内部锁，允许多线程公用一个链接
    check(sqlite3_open_v2(path.c_str(), &m_db,SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,nullptr), m_db);


    //建表
    const char* schema=R"(
        CREATE TABLE IF NOT EXISTS tasks(
            task_id     TEXT PRIMARY KEY,
            url        TEXT NOT NULL,
            status     TEXT NOT NULL,
            error      TEXT DEFAULT '',
            report     TEXT DEFAULT '',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL 
        );
    )";
    check(sqlite3_exec(m_db, schema, nullptr, nullptr, nullptr), m_db);
}

void DB::insertTask(const Task &t)
{
    sqlite3_stmt* stmt = prepare(m_db,
        "INSERT INTO tasks (task_id, url, status, error, report, created_at, updated_at) "
        "VALUES (?, ?, ?, '', '', ?, ?)");

    //防注入攻击
    check(sqlite3_bind_text(stmt, 1, t.id.c_str(),    -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_bind_text(stmt, 2, t.url.c_str(),   -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_bind_text(stmt, 3, t.status.c_str(),-1, SQLITE_TRANSIENT), m_db);
    std::string created = now();
    check(sqlite3_bind_text(stmt, 4, created.c_str(), -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_bind_text(stmt, 5, created.c_str(), -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_step(stmt), m_db);   // 执行
    sqlite3_finalize(stmt);           // 用完销毁
}

void DB::updateStatus(const std::string &id, const std::string &status, const std::string &error)
{
    sqlite3_stmt* stmt = prepare(m_db,
        "UPDATE tasks SET status = ?, error = ?, updated_at = ? WHERE task_id = ?");
    check(sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_bind_text(stmt, 2, error.c_str(),  -1, SQLITE_TRANSIENT), m_db);
    std::string t = now();
    check(sqlite3_bind_text(stmt, 3, t.c_str(),      -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_bind_text(stmt, 4, id.c_str(),     -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_step(stmt), m_db);
    sqlite3_finalize(stmt);
}

void DB::setReport(const std::string &id, const std::string &report)
{
    sqlite3_stmt* stmt = prepare(m_db,
        "UPDATE tasks SET report = ?, status = 'done', error = '', updated_at = ? WHERE task_id = ?");
    check(sqlite3_bind_text(stmt, 1, report.c_str(), -1, SQLITE_TRANSIENT), m_db);
    std::string t = now();
    check(sqlite3_bind_text(stmt, 2, t.c_str(),      -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_bind_text(stmt, 3, id.c_str(),     -1, SQLITE_TRANSIENT), m_db);
    check(sqlite3_step(stmt), m_db);
    sqlite3_finalize(stmt);
}

bool DB::getTask(const std::string &id, Task &out)
{
     sqlite3_stmt* stmt = prepare(m_db,
        "SELECT url, status, error, report FROM tasks WHERE task_id = ?");

    check(sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT), m_db);

    int rc = sqlite3_step(stmt);
    bool found=false;
    if(rc==SQLITE_ROW){
        out.id=id;
        out.id = id;
        out.url    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.error  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        out.report = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        found = true;
    }
    sqlite3_finalize(stmt);
    return false;
}
