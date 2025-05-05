#include "database.hpp"
#include <iostream>

Database::Database()
    : db(nullptr),
      is_open(false) {}

Database::~Database() { close(); }

bool Database::open(const std::string &path) {
    if (is_open)
        close();

    int rc = sqlite3_open(path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    is_open = true;
    return true;
}

void Database::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
    is_open = false;
}

bool Database::isOpen() const { return is_open; }

bool Database::execute(const std::string &sql) {
    if (!is_open)
        return false;

    char *errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool Database::beginTransaction() { return execute("BEGIN TRANSACTION;"); }

bool Database::commitTransaction() { return execute("COMMIT;"); }

bool Database::rollbackTransaction() { return execute("ROLLBACK;"); }

std::vector<std::map<std::string, std::string>>
Database::query(const std::string &sql) {
    std::vector<std::map<std::string, std::string>> result;

    if (!is_open)
        return result;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    int cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::map<std::string, std::string> row;
        for (int i = 0; i < cols; ++i) {
            const char *colName = sqlite3_column_name(stmt, i);
            const char *colValue = (const char *)sqlite3_column_text(stmt, i);

            if (colName && colValue) {
                row[colName] = colValue;
            } else if (colName) {
                row[colName] = "";
            }
        }
        result.push_back(row);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<ColumnInfo> Database::getTableInfo(const std::string &tableName) {
    std::vector<ColumnInfo> columns;

    if (!is_open)
        return columns;

    std::string sql = "PRAGMA table_info(" + tableName + ");";
    auto rows = query(sql);

    for (const auto &row : rows) {
        ColumnInfo col;
        col.name = row.at("name");
        col.type = row.at("type");
        col.not_null = (row.count("notnull") && row.at("notnull") == "1");
        col.primary_key = (row.count("pk") && row.at("pk") == "1");
        columns.push_back(col);
    }

    return columns;
}

std::vector<std::string> Database::getTables() {
    std::vector<std::string> tables;

    if (!is_open)
        return tables;

    auto rows = query("SELECT name FROM sqlite_master WHERE type='table';");

    for (const auto &row : rows) {
        tables.push_back(row.at("name"));
    }

    return tables;
}

std::vector<std::string>
Database::getTableColumns(const std::string &tableName) {
    std::vector<std::string> columns;

    if (!is_open)
        return columns;

    auto infos = getTableInfo(tableName);
    for (const auto &info : infos) {
        columns.push_back(info.name);
    }

    return columns;
}

bool Database::addRecord(const std::string &tableName,
                         const std::map<std::string, std::string> &values) {
    if (!is_open || values.empty())
        return false;

    std::string sql = "INSERT INTO " + tableName + " (";
    std::string valuesPart = ") VALUES (";

    bool first = true;
    for (const auto &pair : values) {
        if (!first) {
            sql += ", ";
            valuesPart += ", ";
        }
        sql += pair.first;
        valuesPart += "'" + pair.second + "'";
        first = false;
    }

    sql += valuesPart + ");";
    return execute(sql);
}

bool Database::updateRecord(const std::string &tableName,
                            const std::map<std::string, std::string> &values,
                            const std::string &where) {
    if (!is_open || values.empty())
        return false;

    std::string sql = "UPDATE " + tableName + " SET ";

    bool first = true;
    for (const auto &pair : values) {
        if (!first) {
            sql += ", ";
        }
        sql += pair.first + " = '" + pair.second + "'";
        first = false;
    }

    if (!where.empty()) {
        sql += " WHERE " + where;
    }

    sql += ";";
    return execute(sql);
}

bool Database::deleteRecord(const std::string &tableName,
                            const std::string &where) {
    if (!is_open)
        return false;

    std::string sql = "DELETE FROM " + tableName;

    if (!where.empty()) {
        sql += " WHERE " + where;
    }

    sql += ";";
    return execute(sql);
}
