#pragma once
#include "./term/terminal.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 4
#define KILO_QUIT_TIMES 3
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

namespace Editor {
    using Term::color;
    using Term::cursor_off;
    using Term::cursor_on;
    using Term::erase_to_eol;
    using Term::fg;
    using Term::Key;
    using Term::move_cursor;
    using Term::style;
    using Term::Terminal;

    enum editorHighlight {
        HL_NORMAL = 0,
        HL_COMMENT,
        HL_MLCOMMENT,
        HL_KEYWORD1,
        HL_KEYWORD2,
        HL_STRING,
        HL_NUMBER,
        HL_MATCH
    };

    struct editorSyntax {
        const char* filetype;
        const char** filematch;
        const char** keywords;
        const char* singleline_comment_start;
        const char* multiline_comment_start;
        const char* multiline_comment_end;
        int flags;
    };

    typedef struct erow {
        int idx;
        int size;
        int rsize;
        char* chars;
        char* render;
        unsigned char* hl;
        int hl_open_comment;
    } erow;

    struct editorConfig {
        int cx, cy;
        int rx;
        int rowoff;
        int coloff;
        int screenrows;
        int screencols;
        int numrows;
        erow* row;
        int dirty;
        char* filename;
        char statusmsg[80];
        time_t statusmsg_time;
        struct editorSyntax* syntax;
    };

    char* editorPrompt(const Terminal& term,
        const char* prompt,
        void (*callback)(char*, int));
    int is_separator(int c);
    void editorUpdateSyntax(erow* row);
    fg editorSyntaxToColor(int hl);
    void editorSelectSyntaxHighlight();
    int editorRowCxToRx(erow* row, int cx);
    int editorRowRxToCx(erow* row, int rx);
    void editorUpdateRow(erow* row);
    void editorInsertRow(int at, const char* s, size_t len);
    void editorFreeRow(erow* row);
    void editorDelRow(int at);
    void editorRowInsertChar(erow* row, int at, int c);
    void editorRowAppendString(erow* row, char* s, size_t len);
    void editorRowDelChar(erow* row, int at);
    void editorInsertChar(int c);
    void editorInsertNewline();
    void editorDelChar();
    char* editorRowsToString(int* buflen);
    void editorOpen(char* filename, std::function<void(std::string, char*)> fn);
    void editorSetStatusMessage(const char* fmt, ...);
    void editorSave(const Terminal& term, 
        std::function<void(std::string, char*, size_t)> fn);
    void editorFindCallback(char* query, int key);
    void editorFind(const Terminal& term);
    void editorScroll();
    void editorDrawRows(std::string& ab);
    void editorDrawStatusBar(std::string& ab);
    void editorDrawMessageBar(std::string& ab);
    void editorRefreshScreen();
    char* editorPrompt(const Terminal& term,
        const char* prompt,
        void (*callback)(char*, int));
    void editorMoveCursor(int key);
    bool editorProcessKeypress(const Terminal& term,
        std::function<void(std::string, char*)> ofn,
        std::function<void(std::string, char*, size_t)> sfn);
    void initEditor(const Terminal& term);
    void editor(Terminal term, std::string file,
        std::function<void(std::string, char*)> ofn,
        std::function<void(std::string, char*, size_t)> sfn);
}