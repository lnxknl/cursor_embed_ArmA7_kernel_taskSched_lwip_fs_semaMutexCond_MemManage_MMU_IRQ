#include "cjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Render a cJSON item/entity/structure to text */
static char *print_number(cJSON *item)
{
    char *str;
    double d = item->valuedouble;
    if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN)
    {
        str = (char*)malloc(21);  /* 2^64+1 can be represented in 21 chars */
        if (str) sprintf(str, "%d", item->valueint);
    }
    else
    {
        str = (char*)malloc(64);  /* This is a nice tradeoff */
        if (str)
        {
            if (fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60)
                sprintf(str, "%.0f", d);
            else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
                sprintf(str, "%e", d);
            else
                sprintf(str, "%f", d);
        }
    }
    return str;
}

static char *print_string(cJSON *item, int isname)
{
    char *str;
    unsigned char *ptr;
    char *ptr2;
    int len = 0;
    unsigned char token;

    if (!item->valuestring)
    {
        str = (char*)malloc(3);
        if (str) strcpy(str, "\"\"");
        return str;
    }

    ptr = (unsigned char*)item->valuestring;
    while ((token = *ptr) && ++len)
    {
        if (strchr("\"\\\b\f\n\r\t", token)) len++;
        else if (token < 32) len += 5;
        ptr++;
    }

    str = (char*)malloc(len + 3);
    if (!str) return 0;

    ptr2 = str;
    ptr = (unsigned char*)item->valuestring;
    *ptr2++ = '\"';
    while (*ptr)
    {
        if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
            *ptr2++ = *ptr++;
        else
        {
            *ptr2++ = '\\';
            switch (token = *ptr++)
            {
                case '\\': *ptr2++ = '\\'; break;
                case '\"': *ptr2++ = '\"'; break;
                case '\b': *ptr2++ = 'b'; break;
                case '\f': *ptr2++ = 'f'; break;
                case '\n': *ptr2++ = 'n'; break;
                case '\r': *ptr2++ = 'r'; break;
                case '\t': *ptr2++ = 't'; break;
                default:
                    sprintf((char*)ptr2, "u%04x", token);
                    ptr2 += 5;
                    break;
            }
        }
    }
    *ptr2++ = '\"';
    *ptr2++ = 0;
    return str;
}

/* Render an array to text */
static char *print_array(cJSON *item, int depth, int fmt)
{
    char **entries;
    char *out = 0, *ptr, *ret;
    int len = 5;
    cJSON *child = item->child;
    int numentries = 0, i = 0, fail = 0;

    /* How many entries in the array? */
    while (child) numentries++, child = child->next;
    /* Explicitly handle numentries == 0 */
    if (!numentries)
    {
        out = (char*)malloc(3);
        if (out) strcpy(out, "[]");
        return out;
    }
    /* Allocate an array to hold the values for each */
    entries = (char**)malloc(numentries * sizeof(char*));
    if (!entries) return 0;
    memset(entries, 0, numentries * sizeof(char*));
    /* Retrieve all the results: */
    child = item->child;
    while (child && !fail)
    {
        ret = print_value(child, depth + 1, fmt);
        entries[i++] = ret;
        if (ret) len += strlen(ret) + 2 + (fmt ? 1 : 0);
        else fail = 1;
        child = child->next;
    }

    /* If we didn't fail, try to malloc the output string */
    if (!fail) out = (char*)malloc(len);
    /* If that fails, we fail */
    if (!out) fail = 1;

    /* Handle failure */
    if (fail)
    {
        for (i = 0; i < numentries; i++) if (entries[i]) free(entries[i]);
        free(entries);
        return 0;
    }

    /* Compose the output array */
    *out = '[';
    ptr = out + 1;
    *ptr = 0;
    for (i = 0; i < numentries; i++)
    {
        strcpy(ptr, entries[i]);
        ptr += strlen(entries[i]);
        if (i != numentries - 1)
        {
            *ptr++ = ',';
            if (fmt) *ptr++ = ' ';
            *ptr = 0;
        }
        free(entries[i]);
    }
    free(entries);
    *ptr++ = ']';
    *ptr++ = 0;
    return out;
} 