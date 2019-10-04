#pragma once

#include <cstring>
#include <string>
#include <vector>

// note: SKPrefs is derived from mec::Preferences, in my MEC GPL project

class SKPrefs {
public:
    enum Type {
        P_NULL,
        P_BOOL,
        P_NUMBER,
        P_STRING,
        P_ARRAY,
        P_OBJECT
    };

    explicit SKPrefs(void *subtree);
    explicit SKPrefs(const std::string &file);
    virtual ~SKPrefs();

    std::vector<std::string> getKeys() const;

    bool getBool(const std::string &v, bool def = false) const;
    int getInt(const std::string &v, int def = 0) const;
    double getDouble(const std::string &v, double def = 0.0) const;
    std::string getString(const std::string &v, const std::string def = "") const;
    Type getType(const std::string &v) const;

    void *getSubTree(const std::string v) const;
    void *getTree() const;

    bool exists(const std::string v) const;
    void print() const;
    bool valid() const;

    class Array {
    public:
        explicit Array(void *);
        ~Array();
        int getSize();
        bool getBool(unsigned i) const;
        int getInt(unsigned i) const;
        double getDouble(unsigned i) const;
        std::string getString(unsigned i) const;
        Type getType(unsigned i) const;
        void *getArray(unsigned i) const;
        void *getObject(unsigned i) const;
        bool valid() const;
    private:
        void *jsonData_;
    };

    void *getArray(const std::string v) const;

private:
    bool loadPreferences(const std::string &file);

    // dont use unique_ptr, as need to use json delete
    void *jsonData_;
    bool owned_;
    bool valid_;
};
