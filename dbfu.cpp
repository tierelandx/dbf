#include <iostream>
#include <fstream>
#include <string>
#include <string.h>

using namespace std;

typedef unsigned char Byte;
typedef unsigned int Byte4;
typedef unsigned short Byte2;

struct dbfField
{
    Byte name[11];
    Byte type;
    Byte4 pos;
    Byte len;
    Byte dec;
    Byte flags;
    Byte reserved1[13];
};

struct dbfHeader
{
    Byte version;
    Byte yymmdd[3];
    Byte4 lastrec;
    Byte2 headlen;
    Byte2 reclen;
    Byte reserved1[16];
    Byte tableflags;
    Byte codepage;
    Byte reserved2[2];
};
class DBF
{
    dbfHeader header;
    dbfField *fields;
    //
    std::ifstream dbf;
    char *buffer;
    long curpos;
    long filesize;
    char message[1024];

public:
    DBF()
    {
        fields = 0;
        buffer = 0;
        curpos = -1;
        filesize = -1;
        strcpy(message, "");
    }
    
    ~DBF()
    {
        close();
    }

    const char *getMessage() const
    {
        return message;
    }
    
    bool open(const char *filename)
    {
        close();
        strcpy(message, "");
        dbf.open(filename, fstream::binary);
        if (!dbf.is_open())
        {
            strcpy(message, "Error open file");
            close();
            return false;
        }

        dbf.seekg(0, dbf.end);
        filesize = dbf.tellg();
        dbf.seekg(0, dbf.beg);
        if (filesize < 64)
        {
            strcpy(message, "File size < 64");
            close();
            return false;
        }

        dbf.read((char *)&header, 32);
        if (!dbf || dbf.gcount() < 32)
        {
            snprintf(message, sizeof(message), "Error read header (%ld)", dbf.gcount());
            close();
            return false;
        }
        size_t size = (header.headlen / 32 - 1) * 32;
        fields = (dbfField *)malloc(size);
        dbf.read((char *)fields, size);
        if (!dbf)
        {
            strcpy(message, "Error read fields");
            close();
            return false;
        }

        int pos = 1;
        for (int n = 0; n < getFieldsCount(); n++)
        {
            fields[n].pos = pos;
            pos += fields[n].len;
        }

        // info();
        buffer = (char *)malloc(getRecLen() + 1);
        // buffer = (char*)malloc(getRecLen()+1);
        if (buffer == 0)
        {
            cerr << "memory error" << endl;
            exit(1);
        }
        // info();

        go(1);
        return fields;
    }

    bool close()
    {
        if (dbf.is_open())
        {
            dbf.close();
        }
        if (fields)
        {
            free(fields);
            fields = 0;
        }
        if (buffer)
        {
            free(buffer);
            buffer = 0;
        }
        curpos = -1;
        filesize = -1;
        memset((void *)&header, 0, 32);
        return true;
    }

    bool is_open() const
    {
        return fields != 0;
    }

    bool is_deleted() const
    {
        return is_open() && *buffer == '*';
    }

    int getFieldsCount() const
    {
        return getHeadLen() / 32 - 1;
    }

    const char *getFieldName(int pos) const
    {
        if (getFieldsCount() > 0)
        {
            return (char *)fields[pos].name;
        }
        return 0;
    }
    
    char getFieldType(int pos) const
    {
        return fields[pos].type;
    }
    
    int getFieldLen(int pos) const
    {
        return fields[pos].len;
    }
    
    int getFieldDec(int pos) const
    {
        return fields[pos].dec;
    }

    int getFieldPos(const char *name) const
    {
        for (int n = 0; n < getFieldsCount(); n++)
        {
            if (strcasecmp((char *)fields[n].name, name) == 0)
            {
                return n;
            }
        }
        return -1;
    }
    
    string getFieldBuffer(const char *name) const
    {
        return getFieldBuffer(getFieldPos(name));
    }
    
    string getFieldValue(const char *name) const
    {
        return getFieldValue(getFieldPos(name));
    }
    
    string getFieldBuffer(int pos) const
    {
        if (pos >= 0 && pos < getFieldsCount())
        {
            return string((char *)buffer + fields[pos].pos, fields[pos].len);
        }
        return string("");
    }

    string getFieldValue(int pos) const
    {
        static char sp[] = " \t\b\r\n";
        string str(getFieldBuffer(pos));
        str.erase(str.find_last_not_of(sp) + 1);
        if (getFieldType(pos) == 'N' || getFieldType(pos) == 'F')
        {
            str.erase(0, str.find_first_not_of(sp));
        }
        return str;
    }

    long getLastRec() const
    {
        return header.lastrec;
    }

    long getFileSize() const
    {
        return filesize;
    }

    int getCharset() const
    {
        return header.codepage;
    }

    int getRecLen() const
    {
        return header.reclen;
    }
    
    int getHeadLen() const
    {
        return header.headlen;
    }

    long go(long pos)
    {
        long oldpos = curpos;
        if (!is_open())
        {
            return oldpos;
        }
        if (pos < 1)
        {
            curpos = 0;
            memset(buffer, ' ', getRecLen());
            return oldpos;
        }
        else if (pos > getLastRec())
        {
            curpos = getLastRec() + 1;
            memset(buffer, ' ', getRecLen());
            return oldpos;
        }
        dbf.seekg(getHeadLen() + getRecLen() * (pos - 1));
        dbf.read((char *)buffer, getRecLen());
        if (!dbf)
        {
            memset(buffer, ' ', getRecLen());
            curpos = -1;
            return oldpos;
        }
        buffer[getRecLen()] = 0;
        curpos = pos;
        return oldpos;
    }
    
    bool skip()
    {
        return skip(1);
    }

    bool skip(int rows)
    {
        go(getPos() + rows);
        return true;
    }

    void info()
    {
        cout << "FileSize: " << getFileSize() << endl;
        cout << "LastRec: " << getLastRec() << endl;
        cout << "RecLen: " << getRecLen() << endl;
        cout << "FieldsCount: " << getFieldsCount() << endl;
        for (int n = 0; n < getFieldsCount(); n++)
        {
            cout << getFieldName(n)
                 << ',' << getFieldType(n)
                 << ',' << (int)fields[n].len
                 << ',' << (int)fields[n].dec
                 << " : " << getFieldLen(n)
                 << ',' << getFieldDec(n)
                 << endl;
        }
    }

    long getPos() const
    {
        return curpos;
    }

    bool bof() const
    {
        if (!is_open())
        {
            return true;
        }
        return getPos() < 1;
    }

    bool eof() const
    {
        if (!is_open() || getPos() < 0)
        {
            return true;
        }
        return getPos() > getLastRec();
    }
    const char *getBuffer() const
    {
        return (char *)buffer;
    }
};

void replace_all(string &str, const string &str1, const string &str2)
{
    size_t pos;
    while ((pos = str.find(str1)) != string::npos)
    {
        str.replace(pos, str1.length(), str2);
    }
}

string json_str(const string &s)
{
    string str(s);
    replace_all(str, "\"", "\\\"");
    replace_all(str, "\t", "\\t");
    replace_all(str, "\r", "\\r");
    replace_all(str, "\n", "\\n");
    return string("\"") + str + '"';
}

string json_str(const char *name, const char *value)
{
    return json_str(name) + ':' + json_str(value);
}

string json_long(const char *name, long value)
{
    return json_str(name) + ':' + (to_string(value));
}

string json_number(const char *name, const char *value)
{
    return json_str(name) + ':' + value;
}

string json_bool(const char *name, bool value)
{
    return json_str(name) + ':' + (value ? "true" : "false");
}

void to_csv(DBF &dbf)
{
    string dd = "\"";
    string csv = ";";
    for (int n = 0; n < dbf.getFieldsCount(); n++)
    {
        cout << (n ? csv : "") << dd << dbf.getFieldName(n)
             << ',' << dbf.getFieldType(n)
             << ',' << dbf.getFieldLen(n)
             << ',' << dbf.getFieldDec(n)
             << dd;
    }
    cout << endl;
    while (!dbf.eof())
    {
        for (int n = 0; n < dbf.getFieldsCount(); n++)
        {
            string str = dbf.getFieldValue(n);
            const char *c_str = str.c_str();
            cout << (n ? csv : "") << dd;
            for (int m = 0; m < str.length(); m++)
            {
                if (c_str[m] == '"')
                {
                    cout << c_str[m];
                }
                cout << c_str[m];
            }
            cout << dd;
        }
        cout << endl;
        dbf.skip();
    }
}

void to_json(DBF &dbf, string &fileName)
{
    string cf = "";
    string cr = "";
    cout << "{"
         << json_str("fileName", fileName.c_str()) << ","
         << json_long("fileSize", dbf.getFileSize()) << ","
         << json_long("lastRec", dbf.getLastRec()) << ","
         << json_long("charset", dbf.getCharset()) << ","
         << endl;

    cout << json_str("fields") << ":[" << endl;
    for (int n = 0; n < dbf.getFieldsCount(); n++)
    {
        cout << (n ? ",\n" : "");
        cout << "{"
             << json_str("fieldName", dbf.getFieldName(n))
             << "," << json_str("type") << ":\"" << dbf.getFieldType(n) << "\""
             << "," << json_str("len") << ":" << dbf.getFieldLen(n)
             << "," << json_str("dec") << ":" << dbf.getFieldDec(n)
             << "}";
    }
    cout << endl
         << "]," << endl;
    cout << "\"rows\":[" << endl;
    while (!dbf.eof())
    {
        cout << cr, cr = ",\n";
        cf = "";
        cout << '{';
        // cout << cf << json_long("#", dbf.getPos()); cf=",";
        // cout << cf << json_bool("deleted", dbf.is_deleted()); cf=",";
        for (int n = 0; n < dbf.getFieldsCount(); n++)
        {
            cout << cf, cf = ",";
            if (dbf.getFieldType(n) == 'N')
            {
                cout << json_number(dbf.getFieldName(n), dbf.getFieldValue(n).c_str());
            }
            // else if (dbf.getFieldType(n) == 'F')
            // {
            //     cout << json_number(dbf.getFieldName(n), dbf.getFieldValue(n) == "T" ? "true" : "false");
            // }
            else
            {
                cout << json_str(dbf.getFieldName(n), dbf.getFieldValue(n).c_str());
            }
        }
        cout << '}';
        // break;
        dbf.skip();
    }
    cout << "\n]}" << endl;
}

int main(int argc, char *argv[])
{
    DBF dbf;
    int i;
    int opt_c = 'c';
    string fileName;

    if (argc < 2)
    {
        cerr << "Usage: dbfu dbf" << endl;
        exit(1);
    }
    for (i = 1; i < argc; i++)
    {
        string opt = argv[i];
        if (opt[0] == '-')
        {
            if (opt == "--json" || opt == "-j")
            {
                opt_c = 'j';
            }
            else if (opt == "--csv" || opt == "-c")
            {
                opt_c = 'c';
            }
            else
            {
                exit(1);
            }
        }
        else
        {
            fileName = argv[i];
        }
    }

    if (!dbf.open(fileName.c_str()))
    {
        cerr << dbf.getMessage() << endl;
        exit(1);
    };

    if (opt_c == 'j')
    {
        to_json(dbf, fileName);
    }
    else
    {
        to_csv(dbf);
    }

    return 0;
}