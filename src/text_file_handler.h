#pragma once

struct Text_File_Handler {
    char *filepath = NULL;
    
    char comment_character = '#';

    bool do_version_number = true;
    bool strip_comments_from_end_of_lines = true;

    char *file_data = NULL;
    char *orig_file_data = NULL;

    int version = -1;

    int line_number = 0;
};

bool start_file(Text_File_Handler *handler, char *filepath);
void end_file(Text_File_Handler *handler);

char *consume_next_line(Text_File_Handler *handler);
void report_error(Text_File_Handler *handler, char *fmt, ...);
