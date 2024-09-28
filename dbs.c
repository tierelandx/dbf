#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef unsigned char Byte;
typedef unsigned int Byte4;
typedef unsigned short Byte2;

typedef struct
{
    Byte name[11];
    Byte type;
    Byte4 pos;
    Byte len;
    Byte dec;
    Byte flags;
    Byte reserved1[13];
} dbfField;

typedef struct
{
    Byte version;
    Byte yy;
    Byte mm;
    Byte dd;
    Byte4 lastrec;
    Byte2 headlen;
    Byte2 reclen;
    Byte reserved1[16];
    Byte tableflags;
    Byte codepage;
    Byte2 fcount;
} dbfHeader;

#define FREE(ptr)  \
    if (ptr)       \
    {              \
        free(ptr); \
        ptr = 0;   \
    }

char *FD = ";";

int main(int argc, char **argv)
{
    dbfHeader hd;
    dbfField *ff;
    FILE *dbf;
    char *buf;
    char *fd = "";
    size_t size;
    long file_size = 0;
    int opt;
    //
    int opt_i = 0;
    int opt_I = 0;
    int opt_r = 0;
    int opt_d = 0;
    int opt_D = 0;
    int opt_H = 0;

    while ((opt = getopt(argc, argv, "hiIrdDH")) != -1)
    {
        switch (opt)
        {
        case 'h':
            fprintf(stderr, "DBf Streamer, 2024\n");
            fprintf(stderr, "dbs options file\n");
            fprintf(stderr, "  - h help \n");
            fprintf(stderr, "  - i out info\n");
            fprintf(stderr, "  - I out info end exit\n");
            fprintf(stderr, "  - r out # record\n");
            fprintf(stderr, "  - d add * field as deleted mark\n");
            fprintf(stderr, "  - D skip deleted records\n");
            exit(2);
            break;
        case 'i':
            opt_i++;
            break;
        case 'I':
            opt_I++;
            break;
        case 'r':
            opt_r++;
            break;
        case 'd':
            opt_d++;
            break;
        case 'D':
            opt_D++;
            break;
        case 'H':
            opt_H++;
            break;
        }
    }

    dbf = fopen(argv[optind], "rb");
    if (dbf == NULL)
    {
        fprintf(stderr, "Error open file (%s)\n", argv[1]);
        exit(1);
    }

    fseek(dbf, 0, SEEK_END);
    file_size = ftell(dbf);
    fseek(dbf, 0, SEEK_SET);

    if (file_size < 65)
    {
        fclose(dbf);
        fprintf(stderr, "Error file size (%ld)\n", file_size);
        exit(1);
    }

    size = fread(&hd, 1, 32, dbf);

    if (size != 32)
    {
        fclose(dbf);
        fprintf(stderr, "Error read header (%ld)\n", size);
        exit(1);
    }
    // Check date values
    if (hd.mm > 12 || hd.mm < 1)
    {
        fclose(dbf);
        fprintf(stderr, "Error month field (%d)\n", hd.mm);
        exit(1);
    }
    if (hd.dd > 31 || hd.dd < 1)
    {
        fclose(dbf);
        fprintf(stderr, "Error day field (%d)\n", hd.mm);
        exit(1);
    }

    hd.fcount = hd.headlen / 32 - 1;

    if (opt_i || opt_I)
    {
        fprintf(stdout, "Version: %d (0x%02X)\n", (int)hd.version, (int)hd.version);
        fprintf(stdout, "Date: %d.%02d.%02d\n", (int)hd.yy + 1900, (int)hd.mm, (int)hd.dd);
        fprintf(stdout, "Lastrec: %ld\n", (long)hd.lastrec);
        fprintf(stdout, "Headlen: %d\n", hd.headlen);
        fprintf(stdout, "Reclen: %d\n", hd.reclen);
        fprintf(stdout, "Charset: %d (0x%02X)\n", hd.codepage, (int)hd.codepage);
        fprintf(stdout, "Fcount: %d \n", hd.fcount);
        fprintf(stdout, "Filesize: %ld \n", file_size);
        fprintf(stdout, "Calcsize: %ld \n", (long) hd.headlen+hd.reclen*hd.lastrec);
        if (opt_I)
        {
            fclose(dbf);
            exit(0);
        }
    }

    ff = malloc(hd.fcount * 32);
    if (ff == NULL)
    {
        fprintf(stderr, "Error fields memory allocate\n");
        exit(1);
    }

    size = fread(ff, 1, hd.fcount * 32, dbf);
    if (size != hd.fcount * 32)
    {
        FREE(ff);
        fclose(dbf);
        fprintf(stderr, "Error read fields (%ld)\n", size);
        exit(1);
    }

    buf = malloc(hd.reclen + 1);
    if (buf == NULL)
    {
        fprintf(stderr, "Error buffer memory allocate\n");
        exit(1);
    }

    if (opt_H)
    {
        // Header info
        for (int f = 0; f < hd.fcount; f++)
        {
            char field_name[12];
            memcpy(field_name, ff[f].name, 11);
            field_name[11] = 0;
            fputs(fd, stdout);
            fprintf(stdout, "\"%s,%c,%d,%d\"", field_name, ff[f].type, ff[f].len, ff[f].dec);
            fd = FD;
        }
        fputc('\n', stdout);
    }

    if (fseek(dbf, hd.headlen, SEEK_SET))
    {
        fprintf(stderr, "Error fseek\n");
        exit(1);
    }

    long record = 0;

    // read DBF file data
    while (fread(buf, 1, hd.reclen, dbf) == hd.reclen)
    {
        char *p = buf;

        record++;
        buf[hd.reclen] = 0;
        fd = "";

        if (*buf == '*' && opt_D)
        {
            continue;
        }

        if (opt_d)
        {
            fprintf(stdout, "\"%c\"", *buf);
            fd = FD;
        }

        if (opt_r)
        {
            fprintf(stdout, "%ld", record);
            fd = FD;
        }

        for (int n = 0; n < hd.fcount; n++)
        {
            int sc = 0; // space counter
            fprintf(stdout, "%s\"", fd);
            for (int m = 0; m < ff[n].len; m++)
            {
                p++;

                if (*p == ' ')
                {
                    sc++;
                    continue;
                }

                if (ff[n].type == 'C')
                {
                    for (int i = 0; i < sc; i++)
                    {
                        fputc(32, stdout);
                    }
                    sc = 0;
                }

                if (*p == '"')
                {
                    fputc(*p, stdout);
                }

                fputc(*p, stdout);
            }
            fprintf(stdout, "\"");
            fd = FD;
        }
        fprintf(stdout, "\n");
    }

    FREE(buf);
    FREE(ff);
    fclose(dbf);

    return 0;
}