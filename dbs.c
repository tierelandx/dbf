#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef VERSION
#define VERSION "0.0.2"
#endif

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

#define FCLOSE(fd)  \
    if (fd)         \
    {               \
        fclose(fd); \
        fd = 0;     \
    }

#define IS_LEAP(y) (((y) % 400 == 0 || (y) % 4 == 0 && (y) % 100 != 0) ? 1 : 0)

char FD[2] = "|"; // field delim char
char FC[2] = "";  // field quota char

void USAGE()
{
    fprintf(stderr, "DBf Streamer, 2024, Version: %s\n", VERSION);
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: dbs [<options>] <file>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h         help \n");
    fprintf(stderr, "  -i         output file info\n");
    fprintf(stderr, "  -I         output file info end exit\n");
    fprintf(stderr, "  -r         output # record\n");
    fprintf(stderr, "  -d         add * field as deleted mark\n");
    fprintf(stderr, "  -D         skip deleted records\n");
    fprintf(stderr, "  -s         trim spaces from fields\n");
    fprintf(stderr, "  -C <char>  field quota char\n");
    fprintf(stderr, "  -c <char>  field delimiter char\n");
    // fprintf(stderr, "  -n         count records only\n");
    exit(2);
}

int main(int argc, char **argv)
{
    dbfHeader hd;
    dbfField *ff = 0;
    FILE *dbf = 0;
    char *buf = 0;
    char *fd = "";
    size_t size;
    long file_size = 0;
    int reclen2 = 1;
    int opt;
    //
    int opt_i = 0;
    int opt_I = 0;
    int opt_r = 0;
    int opt_d = 0;
    int opt_D = 0;
    int opt_H = 0;
    int opt_s = 0;

    if (sizeof(dbfHeader) != 32)
    {
        fprintf(stderr, "Error header size\n");
        exit(1);
    }

    if (sizeof(dbfField) != 32)
    {
        fprintf(stderr, "Error field size\n");
        exit(1);
    }

    if (argc < 2)
    {
        USAGE();
    }

    while ((opt = getopt(argc, argv, "hiIrdDHC:c:s")) != -1)
    {
        switch (opt)
        {
        case 'h':
            USAGE();
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
        case 'C':
            FC[0] = *optarg;
            FC[1] = 0;
            break;
        case 'c':
            FD[0] = *optarg;
            FD[1] = 0;
            break;
        case 's':
            opt_s++;
            break;
        }
    }

    dbf = fopen(argv[optind], "rb");
    if (dbf == NULL)
    {
        fprintf(stderr, "Error open file (%s)\n", argv[1]);
        exit(1);
    }

    // file size
    fseek(dbf, 0, SEEK_END);
    file_size = ftell(dbf);
    fseek(dbf, 0, SEEK_SET);

    if (file_size < 65)
    {
        FCLOSE(dbf);
        fprintf(stderr, "Error file size (%ld)\n", file_size);
        exit(1);
    }

    size = fread(&hd, 1, 32, dbf);

    if (size != 32)
    {
        FCLOSE(dbf);
        fprintf(stderr, "Error read header (%ld)\n", size);
        exit(1);
    }

    hd.fcount = hd.headlen / 32 - 1;

    if (opt_i || opt_I)
    {
        fprintf(stdout, "Version:  %d (0x%02X)\n", (int)hd.version, (int)hd.version);
        fprintf(stdout, "Date:     %d.%02d.%02d\n", (int)hd.yy + 1900, (int)hd.mm, (int)hd.dd);
        fprintf(stdout, "Lastrec:  %ld\n", (long)hd.lastrec);
        fprintf(stdout, "Headlen:  %d\n", hd.headlen);
        fprintf(stdout, "Reclen:   %d\n", hd.reclen);
        fprintf(stdout, "Charset:  %d (0x%02X)\n", hd.codepage, (int)hd.codepage);
        fprintf(stdout, "Fcount:   %d \n", hd.fcount);
        fprintf(stdout, "Filesize: %ld \n", file_size);
        fprintf(stdout, "Calcsize: %ld \n", (long)hd.headlen + hd.reclen * hd.lastrec);
        if (opt_I)
        {
            FCLOSE(dbf);
            exit(0);
        }
    }

    // Simple date check
    if (hd.mm > 12 || hd.mm < 1)
    {
        FCLOSE(dbf);
        fprintf(stderr, "Error month field (%d)\n", hd.mm);
        exit(1);
    }
    if (hd.dd > 31 || hd.dd < 1 || hd.mm == 2 && hd.dd > 29)
    {
        FCLOSE(dbf);
        fprintf(stderr, "Error day field (%d)\n", hd.mm);
        exit(1);
    }

    ff = malloc(hd.fcount * 32);
    if (ff == NULL)
    {
        FCLOSE(dbf);
        fprintf(stderr, "Error fields memory allocate\n");
        exit(1);
    }

    size = fread(ff, 1, hd.fcount * 32, dbf);
    if (size != hd.fcount * 32)
    {
        FREE(ff);
        FCLOSE(dbf);
        fprintf(stderr, "Error read fields (%ld)\n", size);
        exit(1);
    }

    // Sum field length and compare with header 
    for (int i = 0; i < hd.fcount; i++)
    {
        reclen2 += ff[i].len;
    }
    if (hd.reclen != reclen2)
    {
        FREE(ff);
        FCLOSE(dbf);
        fprintf(stdout, "Error record length (%d!=%d)\n",hd.reclen,reclen2);
        exit(1);
    }

    buf = malloc(hd.reclen + 1);
    if (buf == NULL)
    {
        FREE(ff);
        FCLOSE(dbf);
        fprintf(stderr, "Error buffer memory\n");
        exit(EXIT_FAILURE);
    }
    buf[hd.reclen] = 0;

    if (opt_H)
    {
        // Header info
        for (int f = 0; f < hd.fcount; f++)
        {
            char field_name[12];
            memcpy(field_name, ff[f].name, 11);
            field_name[11] = 0;
            fputs(fd, stdout);
            fprintf(stdout, "%s%s,%c,%d,%d%s", FC, field_name, ff[f].type, ff[f].len, ff[f].dec, FC);
            fd = FD;
        }
        fputc('\n', stdout);
    }

    if (fseek(dbf, hd.headlen, SEEK_SET))
    {
        FREE(buf);
        FREE(ff);
        FCLOSE(dbf);
        fprintf(stderr, "Error fseek (%d)\n", hd.headlen);
        exit(1);
    }

    long record = 0;

    // read DBF file data
    while (fread(buf, 1, hd.reclen, dbf) == hd.reclen)
    {
        record++;

        char *p = buf;

        fd = "";

        if (*buf == '*' && opt_D)
        {
            continue;
        }

        if (opt_d)
        {
            fprintf(stdout, "%s%s%c%s", fd, FC, *buf, FC);
            fd = FD;
        }

        if (opt_r)
        {
            fprintf(stdout, "%s%ld", fd, record);
            fd = FD;
        }

        for (int n = 0; n < hd.fcount; n++)
        {
            int sc = 0; // space counter
            fprintf(stdout, "%s%s", fd, FC);
            for (int m = 0; m < ff[n].len; m++)
            {
                p++;

                if (opt_s)
                {
                    // Skip space
                    if (*p == ' ')
                    {
                        sc++;
                        continue;
                    }
                    // Restore inner spaces
                    if (ff[n].type == 'C')
                    {
                        for (int i = 0; i < sc; i++)
                        {
                            fputc(32, stdout);
                        }
                        sc = 0;
                    }
                }

                if (*p == *FC)
                {
                    fputc(*p, stdout);
                }

                fputc(*p, stdout);
            }
            fprintf(stdout, "%s", FC);
            fd = FD;
        }
        fprintf(stdout, "\n");
    }
    //    fprintf(stdout,"%ld record(s)\n",record);

    FREE(buf);
    FREE(ff);
    FCLOSE(dbf);

    return EXIT_SUCCESS;
}