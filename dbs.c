#include <stdio.h>
#include <stdlib.h>

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
    Byte yymmdd[3];
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
    size_t size;
    char *buf;

    dbf = fopen(argv[1], "rb");
    if (dbf == NULL)
    {
        fprintf(stderr, "Error open file (%s)\n", argv[1]);
        exit(1);
    }

    size = fread(&hd, 1, 32, dbf);

    if (size != 32)
    {
        fclose(dbf);
        fprintf(stderr, "Error read header (%ld)\n", size);
        exit(1);
    }
    hd.fcount = hd.headlen / 32 - 1;

    ff = malloc(hd.fcount * 32);
    size = fread(ff, 1, hd.fcount * 32, dbf);
    if (size != hd.fcount * 32)
    {
        FREE(ff);
        fclose(dbf);
        fprintf(stderr, "Error read fields (%ld)\n", size);
        exit(1);
    }
    buf = malloc(hd.reclen + 1);

    // Header info
    char *fd = "";
    for (int f = 0; f < hd.fcount; f++)
    {
        fputs(fd, stdout);
        fprintf(stdout, "\"%s,%c,%d,%d\"", ff[f].name, ff[f].type, ff[f].len, ff[f].dec);
        fd = FD;
    }
    fputc(10, stdout);

    fseek(dbf, hd.headlen, SEEK_SET);

    long record = 0;

    // file data
    while (fread(buf, 1, hd.reclen, dbf) == hd.reclen)
    {
        char *p = buf;

        record++;
        buf[hd.reclen] = 0;
        fd = "";

        fprintf(stdout, "%ld", record);
        fd = FD;

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