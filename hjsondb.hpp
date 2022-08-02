/*
                     ,----------------,              ,---------,
                ,-----------------------,          ,"        ,"|
              ,"                      ,"|        ,"        ,"  |
             +-----------------------+  |      ,"        ,"    |
             |  .-----------------.  |  |     +---------+      |
             |  | huhai@ubuntu:   |  |  |     | -==----'|      |
             |  |                 |  |  |     |         |      |
             |  |                 |  |  |/----|`---=    |      |
             |  |                 |  |  |   ,/|==== ooo |      ;
             |  |                 |  |  |  // |(((( [33]|    ,"
             |  `-----------------'  |," .;'| |((((     |  ,"
             +-----------------------+  ;;  | |         |,"
                /_)______________(_/  //'   | +---------+
           ___________________________/___  `,
          /  oooooooooooooooo  .o.  oooo /,   ,"-----------
         / ==ooooooooooooooo==.o.  ooo= //   ,`--{)B     ,"

 * @Date:  2021-09-09
 * @LastEditTime:  2021-09-09
 * @LastEditors:
 * @FilePath:
 * @Description:
 */
#ifndef JSONDB_H
#define JSONDB_H
#include <string>
#include <fstream>
#include "json.hpp"
// #include "hhutils.h"
class HJsonDB
{
    std::string dbPath;

    nlohmann::json loadFromFile(const std::string &path)
    {
        std::ifstream f(path);
        if (f.is_open())
        {
            std::string text;
            std::string line;
            while (getline(f, line))
            {
                text += line + "\n";
            }
            f.close();
            try
            {
                auto j = nlohmann::json::parse(text);
                return j;
            }
            catch (const nlohmann::detail::exception &err)
            {
                printf("db parse json failed:%s", err.what());
            }
        }
        return nullptr;
    }

    bool saveFile( const nlohmann::json &js, const std::string &path)
    {
        auto j = js.dump(4);
        std::ofstream f(path);
        if (f.is_open())
        {
            f << j;
            f.flush();
            f.close();
            return true;
        }
        return false;
    }

public:
    HJsonDB(const std::string &path) : dbPath(path) {}
    HJsonDB(){}
    nlohmann::json cache;
    bool load()
    {
        cache = loadFromFile(dbPath);
        return cache != nullptr;
    }

    template<class T>
    bool loadFromFile(const std::string &jfile, T &out)
    {
        auto j = loadFromFile(jfile);
        if(j == nullptr)
        {
            return false;
        }
        try
        {
            /* code */
            out = j;
            return true;
        }
        catch(const std::exception& e)
        {
            printf("loadFromFile error:%s\n", e.what()); 
            return false;
        }
    }

    template<class T>
    bool saveToFile(const T &out, const std::string &jfile)
    {
        try
        {
            nlohmann::json j = out;
            return saveFile(j, jfile);
        }
        catch(const std::exception& e)
        {
            printf("loadFromFile error:%s\n", e.what()); 
            return false;
        }
    }

    uint64_t lastModifyTime()
    {
        // return hhutils::hutils::getFileLastChangeTime(dbPath);
    }

    bool save()
    {
        return saveFile(cache, dbPath);
    }
};

#endif // JSONDB_H
