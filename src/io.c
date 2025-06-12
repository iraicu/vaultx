#include "io.h"

int rename_file(const char *old_name, const char *new_name)
{
    // Attempt to rename the file
    if (rename(old_name, new_name) != 0)
    {
        // If rename fails, perror prints a descriptive error message
        perror("Error renaming file");
        return -1;
    }

    // Success
    return 0;
}

void remove_file(const char *fileName)
{
    // Attempt to remove the file
    if (remove(fileName) == 0)
    {
        if (DEBUG)
            printf("File '%s' removed successfully.\n", fileName);
    }
    else
    {
        perror("Error removing file");
    }
}

int move_file_overwrite(const char *source_path, const char *destination_path)
{
    if (DEBUG)
        printf("move_file_overwrite()...\n");
    // Attempt to rename the file
    if (rename(source_path, destination_path) == 0)
    {
        return 0;
    }

    // If rename failed, check if it's due to cross-device move
    if (errno != EXDEV)
    {
        perror("Error renaming file");
        return -1;
    }

    // Proceed to copy and delete since it's a cross-filesystem move

    // Remove the destination file if it exists to allow overwriting
    if (remove(destination_path) != 0 && errno != ENOENT)
    {
        perror("Error removing existing destination file");
        return -1;
    }

    // Continue with copying as in the original move_file function
    FILE *source = fopen(source_path, "rb");
    if (source == NULL)
    {
        perror("Error opening source file for reading");
        return -1;
    }

    FILE *destination = fopen(destination_path, "wb");
    if (destination == NULL)
    {
        perror("Error opening destination file for writing");
        fclose(source);
        return -1;
    }

    if (!BENCHMARK)
        printf("deep copy started...\n");
    size_t buffer_size = 1024 * 1024 * 8; // 1 MB
    uint8_t *buffer = (uint8_t *)calloc(buffer_size, 1);

    if (buffer == NULL)
    {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0)
    {
        size_t bytes_written = fwrite(buffer, 1, bytes, destination);
        if (bytes_written != bytes)
        {
            perror("Error writing to destination file");
            fclose(source);
            fclose(destination);
            return -1;
        }
    }

    if (ferror(source))
    {
        perror("Error reading from source file");
        fclose(source);
        fclose(destination);
        return -1;
    }

    if (fflush(source) != 0)
    {
        perror("Failed to flush buffer");
        fclose(source);
        return EXIT_FAILURE;
    }

    if (fsync(fileno(source)) != 0)
    {
        perror("Failed to fsync buffer");
        fclose(source);
        return EXIT_FAILURE;
    }

    fclose(source);

    if (fflush(destination) != 0)
    {
        perror("Failed to flush buffer");
        fclose(destination);
        return EXIT_FAILURE;
    }

    if (fsync(fileno(destination)) != 0)
    {
        perror("Failed to fsync buffer");
        fclose(destination);
        return EXIT_FAILURE;
    }

    fclose(destination);

    if (remove(source_path) != 0)
    {
        perror("Error deleting source file after moving");
        return -1;
    }

    if (!BENCHMARK)
        printf("deep copy finished!\n");
    if (DEBUG)
        printf("move_file_overwrite() finished!\n");
    return 0; // Success
}

long get_file_size(const char *filename)
{
    FILE *file = fopen(filename, "rb"); // Open the file in binary mode
    long size;

    if (file == NULL)
    {
        printf("Error opening file %s (#2)\n", filename);

        perror("Error opening file");
        return -1;
    }

    // Move the file pointer to the end of the file
    if (fseek(file, 0, SEEK_END) != 0)
    {
        perror("Error seeking to end of file");
        fclose(file);
        return -1;
    }

    // Get the current position in the file, which is the size
    size = ftell(file);
    if (size == -1L)
    {
        perror("Error getting file position");
        fclose(file);
        return -1;
    }

    fclose(file);
    return size;
}