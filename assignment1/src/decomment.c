#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/* This is skeleton code for reading characters from
standard input (e.g., a file or console input) one by one until
the end of the file (EOF) is reached. It keeps track of the current
line number and is designed to be extended with additional
functionality, such as processing or transforming the input data.
In this specific task, the goal is to implement logic that removes
C-style comments from the input. */

enum State
{
  BEGIN,
  SLASH,
  SINGLE_LINE_COMMENT,
  MULTI_LINE_COMMENT,
  STAR,
  IN_STRING,
  IN_CHAR
};

enum State handle_begin(char ch) // initial state
{
  if (ch == '/')
    return SLASH;

  else if (ch == '\'')
  {
    printf("\'");
    return IN_CHAR;
  }

  else if (ch == '"')
  {
    printf("\"");
    return IN_STRING;
  }

  else
  {
    printf("%c", ch);
    return BEGIN;
  }
}

enum State handle_slash(char ch, int line_cur, int *line_com) // meet first /
{
  if (ch == '/')
  {
    printf(" ");
    return SINGLE_LINE_COMMENT;
  }

  else if (ch == '*')
  {
    printf(" ");
    *line_com = line_cur;
    return MULTI_LINE_COMMENT;
  }

  else
  {
    printf("/%c", ch);
    return BEGIN;
  }
}

enum State handle_single_line_comment(char ch) // meet two /
{
  if (ch == '\n')
  {
    printf("\n");
    return BEGIN;
  }

  return SINGLE_LINE_COMMENT;
}

enum State handle_multi_line_comment(char ch) // meet / and *
{
  if (ch == '\n')
  {
    printf("\n");
    return MULTI_LINE_COMMENT;
  }

  if (ch == '*')
    return STAR;

  return MULTI_LINE_COMMENT;
}

enum State handle_star(char ch) // meet * after /*
{
  if (ch == '/')
    return BEGIN;

  if (ch == '*')
    return STAR;

  if (ch == '\n')
  {
    printf("\n");
    return MULTI_LINE_COMMENT;
  }

  return MULTI_LINE_COMMENT;
}

enum State handle_in_char(char ch) // meet '
{
  printf("%c", ch);

  if (ch == '\'')
    return BEGIN;

  return IN_CHAR;
}

enum State handle_in_string(char ch) // meet "
{
  printf("%c", ch);
  if (ch == '\"')
    return BEGIN;
  return IN_STRING;
}

int main(void)
{
  // ich: int type variable to store character input from getchar() (abbreviation of int character)
  int ich;
  // line_cur & line_com: current line number and comment line number (abbreviation of current line and comment line)
  int line_cur, line_com;
  // ch: character that comes from casting (char) on ich (abbreviation of character)
  char ch;

  // Initial State
  enum State s = BEGIN;
  line_cur = 1;
  line_com = -1;

  // This while loop reads all characters from standard input one by one
  while (1)
  {
    ich = getchar();

    if (ich == EOF)
    {
      if (s == MULTI_LINE_COMMENT || s == STAR)
      {
        fprintf(stderr, "Error: line %d: unterminated comment\n", line_com);
        return EXIT_FAILURE;
      }
      break;
    }

    ch = (char)ich;

    switch (s)
    {
    case BEGIN:
      s = handle_begin(ch);
      break;

    case SLASH:
      s = handle_slash(ch, line_cur, &line_com);
      break;

    case SINGLE_LINE_COMMENT:
      s = handle_single_line_comment(ch);
      break;

    case MULTI_LINE_COMMENT:
      s = handle_multi_line_comment(ch);
      break;

    case STAR:
      s = handle_star(ch);
      break;

    case IN_CHAR:
      s = handle_in_char(ch);
      break;

    case IN_STRING:
      s = handle_in_string(ch);
      break;
    }

    if (ch == '\n')
      line_cur++;
  }

  if (s == MULTI_LINE_COMMENT || s == STAR)
  {
    fprintf(stderr, "Error: line %d: unterminated comment\n", line_com);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
