#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <kernel/fs/fs.h>


/**
 * Formats a path by contatenating it with the cwd while removing any dot directories.
 * The path can be absolute or relative.
 * 
 * If the path is absolute, cwd is ignored.
 * If the given path is relative, the cwd is expected to be an absolute path
 * shorter than the specified maximum length and NOT have a trailing slash.
 * 
 * The null character is always appended, even if its offset is max_len.
 * 
 * @param src the original path
 * @param cwd the current working directory
 * @param dst a buffer to place the formatted path
 * @param max_len the maximum formatted path length
 * 
 * @return the formatted path length, or 0 if formatted path length exceeds max_len
 */
size_t format_path(const char* src, const char* cwd, char* dst, size_t max_len);

/**
 * Returns true if the given filename is valid.
 * A filename is considered valid if it doesn't contain any of the specified
 * forbidden characters or exceeds the specified maximum length.
 * A filename is also considered invalid if corresponds to any of the dot directory names.
 * 
 * @param filename the filename to check the validity of
 * @param forbidden_chars the forbidden characters
 * @param max_len the maximum length (null character included)
 * 
 * @return true if the path in the given directory array is valid
 */
bool is_valid_filename(const char* filename, const char* forbidden_chars, size_t max_len);

/**
 * Returns true if the given path is valid.
 * A path is considered valid if it none of its directories contain any of the specified
 * forbidden characters or exceed their specified maximum length.
 * 
 * If PATH_SEPARATOR is included in the forbidden chars, it will be ignored.
 * 
 * @param path the path to be checked
 * @param forbidden_chars the forbidden characters
 * @param max_name_len the maximum filename length
 * 
 * @return true if the path in the given directory array is valid
 */
bool is_valid_path(const char* path, const char* forbidden_chars, size_t max_name_len);
