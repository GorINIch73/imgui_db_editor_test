#pragma once

#include <functional>
#include <map>
#include <sqlite3.h>
#include <string>
#include <vector>

struct ColumnInfo {
        std::string name;
        std::string type;
        bool not_null;
        bool primary_key;
};

class Database {
    public:
        Database();
        ~Database();

        bool open(const std::string &path);
        void close();
        bool isOpen() const;

        bool execute(const std::string &sql);
        bool beginTransaction();
        bool commitTransaction();
        bool rollbackTransaction();

        std::vector<std::map<std::string, std::string>>
        query(const std::string &sql);
        std::vector<ColumnInfo> getTableInfo(const std::string &tableName);

        std::vector<std::string> getTables();
        std::vector<std::string> getTableColumns(const std::string &tableName);

        bool addRecord(const std::string &tableName,
                       const std::map<std::string, std::string> &values);
        bool updateRecord(const std::string &tableName,
                          const std::map<std::string, std::string> &values,
                          const std::string &where);
        bool deleteRecord(const std::string &tableName,
                          const std::string &where);

    private:
        sqlite3 *db;
        bool is_open;
};
