//--------------------------------------------------------------------------------------------------
// System Programming                         I/O Lab                                     FALL 2025
//
/// @file
/// @brief resursively traverse directory tree and list all entries
/// @author <yourname>
/// @studid <studentid>
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>

/// @brief output control flags
#define F_DEPTH 0x1  ///< print directory tree
#define F_Filter 0x2 ///< pattern matching

/// @brief maximum numbers
#define MAX_DIR 64         ///< maximum number of supported directories
#define MAX_PATH_LEN 1024  ///< maximum length of a path
#define MAX_DEPTH 20       ///< maximum depth of directory tree (for -d option)
int max_depth = MAX_DEPTH; ///< maximum depth of directory tree (for -d option)

/// @brief struct holding the summary
struct summary
{
  unsigned int dirs;  ///< number of directories encountered
  unsigned int files; ///< number of files
  unsigned int links; ///< number of links
  unsigned int fifos; ///< number of pipes
  unsigned int socks; ///< number of sockets

  unsigned long long size;   ///< total size (in bytes)
  unsigned long long blocks; ///< total number of blocks (512 byte blocks)
};

/// @brief print strings used in the output
const char *print_formats[8] = {
    "Name                                                        User:Group           Size    Blocks Type\n",
    "----------------------------------------------------------------------------------------------------\n",
    "%s  ERROR: %s\n",
    "%-54s  No such file or directory\n",
    "%-54s  %8.8s:%-8.8s  %10llu  %8llu    %c\n",
    "Invalid pattern syntax",
};
const char *pattern = NULL; ///< pattern for filtering entries

/// @brief abort the program with EXIT_FAILURE and an optional error message
///
/// @param msg optional error message or NULL
/// @param format optional format string (printf format) or NULL
void panic(const char *msg, const char *format)
{
  if (msg)
  {
    if (format)
      fprintf(stderr, format, msg);
    else
      fprintf(stderr, "%s\n", msg);
  }
  exit(EXIT_FAILURE);
}

/// @brief read next directory entry from open directory 'dir'. Ignores '.' and '..' entries
///
/// @param dir open DIR* stream
/// @retval entry on success
/// @retval NULL on error or if there are no more entries
struct dirent *get_next(DIR *dir)
{
  struct dirent *next;
  int ignore;

  do
  {
    errno = 0;
    next = readdir(dir);
    if (errno != 0)
      perror(NULL);
    ignore = next && ((strcmp(next->d_name, ".") == 0) || (strcmp(next->d_name, "..") == 0));
  } while (next && ignore);

  return next;
}

/// @brief qsort comparator to sort directory entries. Sorted by name, directories first.
///
/// @param a pointer to first entry
/// @param b pointer to second entry
/// @retval -1 if a<b
/// @retval 0  if a==b
/// @retval 1  if a>b
static int dirent_compare(const void *a, const void *b)
{
  struct dirent *e1 = *(struct dirent **)a;
  struct dirent *e2 = *(struct dirent **)b;

  // if one of the entries is a directory, it comes first
  if (e1->d_type != e2->d_type)
  {
    if (e1->d_type == DT_DIR)
      return -1;
    if (e2->d_type == DT_DIR)
      return 1;
  }

  // otherwise sort by name
  return strcmp(e1->d_name, e2->d_name);
}

// 패턴 매칭 함수
int pattern_match(const char *s, const char *p) // s : 파일 이름, p : 패턴
{
  // 끝까지 둘 다 도달했으면 성공
  if (*p == '\0' && *s == '\0')
    return 1;

  // 패턴 끝났는데 문자열 남으면 실패
  if (*p == '\0')
    return 0;

  // 현재 패턴 글자
  if (*p == '?')
  {
    if (pattern_match(s, p + 1))
      return 1; // ?가 0글자 매칭

    if (*s && pattern_match(s + 1, p))
      return 1; // ?가 여러 글자 매칭

    if (*s && pattern_match(s + 1, p + 1))
      return 1; // ?가 1글자 매칭

    return 0;
  }

  else if (*p == '*')
  {
    // 직전 글자/그룹 반복
    for (int i = 0;; i++)
    {
      if (pattern_match(s + i, p + 1))
        return 1;

      if (s[i] == '\0')
        break;
    }

    return 0;
  }

  else if (*p == '(')
  {
    // 그룹 시작
    const char *end = strchr(p, ')');
    if (!end)
      return 0; // 닫는 괄호 없으면 실패

    // 그룹 문자열 추출
    int glen = end - (p + 1);
    char group[64];
    strncpy(group, p + 1, glen);
    group[glen] = '\0';

    // 그룹 매칭 시도
    if (strncmp(s, group, glen) == 0)
      // 그룹 매칭 성공, 남은 패턴 이어서 확인
      return pattern_match(s + glen, end + 1);

    else
      return 0;
  }

  else
  {
    // 그냥 글자 비교
    if (*s == *p)
    {
      return pattern_match(s + 1, p + 1);
    }
    else
    {
      return 0;
    }
  }
}

/// @brief recursively process directory @a dn and print its tree
///
/// @param dn absolute or relative path string
/// @param pstr prefix string printed in front of each entry
/// @param stats pointer to statistics
/// @param flags output control flags (F_*)
int process_dir(const char *dn, const char *pstr, struct summary *stats, unsigned int flags, int depth)
{
  // 깊이 제한 검사
  if ((flags & F_DEPTH) && depth > max_depth)
    return 0;

  DIR *dir;
  struct dirent *entry;
  char path[MAX_PATH_LEN];
  struct stat st;

  dir = opendir(dn);

  if (dir == NULL)
  {
    printf("  ERROR: %s\n", strerror(errno)); // 디렉토리 열기 실패 시 출력
    return 0;
  }

  struct dirent *list[1024];
  int count = 0;

  while ((entry = get_next(dir)) != NULL)
  {
    list[count] = malloc(sizeof(struct dirent)); // 엔트리 복사
    memcpy(list[count], entry, sizeof(struct dirent));
    count++;
  }

  closedir(dir);

  qsort(list, count, sizeof(struct dirent *), dirent_compare); // 정렬

  int matched_any = 0; // 매칭 여부

  for (int i = 0; i < count; i++)
  {
    entry = list[i];
    snprintf(path, sizeof(path), "%s/%s", dn, entry->d_name);

    if (lstat(path, &st) == -1)
    {
      free(entry);
      continue;
    }

    if (S_ISDIR(st.st_mode))
    {
      // 디렉토리면 재귀 탐색
      char newprefix[MAX_PATH_LEN];
      snprintf(newprefix, sizeof(newprefix), "%s  ", pstr);

      int child_match = process_dir(path, newprefix, stats, flags, depth + 1);

      if (child_match)
      {
        // 자식 중 매칭 있으면 디렉토리 출력
        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);
        const char *uname = pw ? pw->pw_name : "unknown";
        const char *gname = gr ? gr->gr_name : "unknown";

        printf("%s%-54s  %8.8s:%-8.8s  %10llu  %8llu    d\n",
               pstr, entry->d_name,
               uname, gname,
               (unsigned long long)st.st_size,
               (unsigned long long)st.st_blocks);

        stats->dirs++;
        stats->size += st.st_size;
        stats->blocks += st.st_blocks;

        matched_any = 1;
      }
    }
    else
    {
      int ok = 1;
      if ((flags & F_Filter) && pattern != NULL)
      {
        if (!pattern_match(entry->d_name, pattern)) // 패턴 매칭 검사
          ok = 0;
      }

      if (ok)
      {
        char typechar = ' ';
        if (S_ISLNK(st.st_mode))
        {
          typechar = 'l';
          stats->links++;
        }
        else if (S_ISFIFO(st.st_mode))
        {
          typechar = 'f';
          stats->fifos++;
        }
        else if (S_ISSOCK(st.st_mode))
        {
          typechar = 's';
          stats->socks++;
        }
        else
        {
          stats->files++;
        }

        stats->size += st.st_size;
        stats->blocks += st.st_blocks;

        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);
        const char *uname = pw ? pw->pw_name : "unknown";
        const char *gname = gr ? gr->gr_name : "unknown";

        printf("%s%-54s  %8.8s:%-8.8s  %10llu  %8llu    %c\n",
               pstr, entry->d_name,
               uname, gname,
               (unsigned long long)st.st_size,
               (unsigned long long)st.st_blocks,
               typechar);

        matched_any = 1;
      }
    }

    free(entry);
  }

  return matched_any; // 매칭 있으면 1
}

/// @brief print program syntax and an optional error message. Aborts the program with EXIT_FAILURE
///
/// @param argv0 command line argument 0 (executable)
/// @param error optional error (format) string (printf format) or NULL
/// @param ... parameter to the error format string
void syntax(const char *argv0, const char *error, ...)
{
  if (error)
  {
    va_list ap;

    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);

    printf("\n\n");
  }

  assert(argv0 != NULL);

  fprintf(stderr, "Usage %s [-d depth] [-f pattern] [-h] [path...]\n"
                  "Recursively traverse directory tree and list all entries. If no path is given, the current directory\n"
                  "is analyzed.\n"
                  "\n"
                  "Options:\n"
                  " -d depth   | set maximum depth of directory traversal (1-%d)\n"
                  " -f pattern | filter entries using pattern (supports \'?\', \'*\', and \'()\')\n"
                  " -h         | print this help\n"
                  " path...    | list of space-separated paths (max %d). Default is the current directory.\n",
          basename(argv0), MAX_DEPTH, MAX_DIR);

  exit(EXIT_FAILURE);
}

/// @brief program entry point
int main(int argc, char *argv[])
{
  //
  // default directory is the current directory (".")
  //
  const char CURDIR[] = ".";
  const char *directories[MAX_DIR];
  int ndir = 0;

  struct summary tstat = {0}; // a structure to store the total statistics
  unsigned int flags = 0;

  //
  // parse arguments
  //
  for (int i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      if (!strcmp(argv[i], "-d"))
      {
        flags |= F_DEPTH;

        if (++i < argc && argv[i][0] != '-')
        {
          max_depth = atoi(argv[i]);

          if (max_depth < 1 || max_depth > MAX_DEPTH)
            syntax(argv[0], "Invalid depth value '%s'. Must be between 1 and %d.", argv[i], MAX_DEPTH);
        }

        else
          syntax(argv[0], "Missing depth value argument.");
      }

      else if (!strcmp(argv[i], "-f"))
      {
        if (++i < argc && argv[i][0] != '-')
        {
          flags |= F_Filter;
          pattern = argv[i]; // 필터링 패턴 저장
        }

        else
          syntax(argv[0], "Missing filtering pattern argument.");
      }

      else if (!strcmp(argv[i], "-h"))
        syntax(argv[0], NULL);

      else
        syntax(argv[0], "Unrecognized option '%s'.", argv[i]);
    }
    else
    {
      if (ndir < MAX_DIR)
      {
        const char *arg = argv[i];
        size_t len = strlen(arg);

        // .tree 확장자 그대로 출력 (compare.sh 맞추기 위함)
        if (len > 5 && strcmp(arg + len - 5, ".tree") == 0)
          directories[ndir++] = argv[i];

        else
          directories[ndir++] = argv[i]; // 그냥 디렉토리 이름
      }

      else
        fprintf(stderr, "Warning: maximum number of directories exceeded, ignoring '%s'.\n", argv[i]);
    }
  }

  // if no directory was specified, use the current directory
  if (ndir == 0)
    directories[ndir++] = CURDIR;

  //
  // process each directory
  //
  for (int i = 0; i < ndir; i++)
  {
    struct summary dstat = {0};

    printf("%s", print_formats[0]);
    printf("%s", print_formats[1]);

    printf("%s\n", directories[i]); // 디렉토리/트리 이름 출력

    process_dir(directories[i], "  ", &dstat, flags, 0);

    printf("%s", print_formats[1]);

    const char *f_s = (dstat.files == 1) ? "" : "s";
    const char *d_s = (dstat.dirs == 1) ? "y" : "ies";
    const char *l_s = (dstat.links == 1) ? "" : "s";
    const char *p_s = (dstat.fifos == 1) ? "" : "s";
    const char *s_s = (dstat.socks == 1) ? "" : "s";

    char summary[128];
    snprintf(summary, sizeof(summary),
             "%u file%s, %u director%s, %u link%s, %u pipe%s, and %u socket%s",
             dstat.files, f_s,
             dstat.dirs, d_s,
             dstat.links, l_s,
             dstat.fifos, p_s,
             dstat.socks, s_s);

    printf("%-68s %14llu %9llu\n",
           summary,
           (unsigned long long)dstat.size,
           (unsigned long long)dstat.blocks);

    printf("\n");

    tstat.files += dstat.files;
    tstat.dirs += dstat.dirs;
    tstat.links += dstat.links;
    tstat.fifos += dstat.fifos;
    tstat.socks += dstat.socks;
    tstat.size += dstat.size;
    tstat.blocks += dstat.blocks;
  }

  //
  // print aggregate statistics if more than one directory was traversed
  //
  if (ndir > 1)
  {
    printf("Analyzed %d directories:\n"
           "  total # of files:        %16d\n"
           "  total # of directories:  %16d\n"
           "  total # of links:        %16d\n"
           "  total # of pipes:        %16d\n"
           "  total # of sockets:      %16d\n"
           "  total # of entries:      %16d\n"
           "  total file size:         %16llu\n"
           "  total # of blocks:       %16llu\n",
           ndir,
           tstat.files, tstat.dirs, tstat.links, tstat.fifos, tstat.socks,
           tstat.files + tstat.dirs + tstat.links + tstat.fifos + tstat.socks,
           tstat.size, tstat.blocks);
  }

  //
  // that's all, folks!
  //
  return EXIT_SUCCESS;
}
