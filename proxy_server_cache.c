#include "proxy_parse.h";
#include <stdio.h>;
#include <string.h>;
#include <time.h>;

const int PORT = 8080;
typedef struct cache_element cache_element;

struct cache_element
{
  char *data;
  int len;
  char *url;
  time_t lru_time_track;
  cache_element *next;
};

cache_element *find_cache_element(char *url);
int add_cache_element(char *data, int size, char url);
void remove_cache_element();

