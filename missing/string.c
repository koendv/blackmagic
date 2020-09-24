#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

int strncasecmp(const char *s1, const char *s2, size_t n) {
  int r = 0;
  while (n &&
         ((s1 == s2) || !(r = ((int)(tolower(*((unsigned char *)s1)))) -
                              tolower(*((unsigned char *)s2)))) &&
         (--n, ++s2, *s1++))
    ;
  return r;
}

#if 0
char *strncpy(char *s1, const char *s2, size_t n) {
  char *s = s1;
  while (n) {
    if ((*s = *s2) != 0)
      s2++; /* Need to fill tail with 0s. */
    ++s;
    --n;
  }
  return s1;
}
#endif

char *(strtok_r)(char *s, const char *delimiters, char **lasts) {
  char *sbegin, *send;
  sbegin = s ? s : *lasts;
  sbegin += strspn(sbegin, delimiters);
  if (*sbegin == '\0') {
    *lasts = "";
    return NULL;
  }
  send = sbegin + strcspn(sbegin, delimiters);
  if (*send != '\0')
    *send++ = '\0';
  *lasts = send;
  return sbegin;
}

char *(strtok)(char *restrict s1, const char *restrict delimiters) {
  static char *ssave = "";
  return strtok_r(s1, delimiters, &ssave);
}

