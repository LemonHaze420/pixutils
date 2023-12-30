/*
    PIXUtils v1

    Written by LemonHaze (Dylan) - 09/12/2023
*/

#include <iostream>
#include <filesystem>

#include <ostream>
#include <fstream>
#include <vector>

#include <zlib.h>

using namespace std;

namespace fs = std::filesystem;

// compresses all data into a single chunk
#define COMPRESS_ALL

// when not compressing into a single chunk,
// only chunk files up to this max. size.
#define MAX_CHUNK_SIZE 1048576

class Pix {
public:

    struct Entry {
        int compressed_size = 0;
        int decompressed_size = 0;

        vector<unsigned char> data;
    };    
    vector<Entry> entries;

    struct FileEntry {
        fs::path path;
        vector<char> data;
    };
    vector<FileEntry> fileEntries;


    Pix()
    {
        entries.clear();
    }

    // @TODO: Needs fixing, always off when compared to originals
    void write(std::string levelName, std::filesystem::path path)
    {
        fs::path rtdDir = path;
        rtdDir.append("rtd/");

        if (!fs::exists(rtdDir))
        {
            printf("rtd dir does not exist\n");
            return;
        }
        else
        {
            size_t totalContentSize = 0;

            // collect all entries
            size_t headerSize = 0;
            for (auto& p : filesystem::recursive_directory_iterator(path))
            {
                if (!p.is_regular_file())
                    continue;

                ifstream ifs(p.path(), ios::binary);
                if (ifs.good())
                {
                    auto RelPath = fs::relative(p.path(), path).string();

                    printf("Adding '%s'...", RelPath.c_str());
                    ifs.seekg(0, ios::end);
                    size_t eof = ifs.tellg();
                    ifs.seekg(0, ios::beg);

                    FileEntry entry;
                    entry.path = RelPath;
                    entry.data.resize(eof);
                    ifs.read(entry.data.data(), eof);

                    printf("(size: 0x%X).\n", (int)entry.data.size());
                    totalContentSize += entry.data.size();

                    size_t str_len = strlen(RelPath.c_str());
                    headerSize += str_len + 1 + 4;

                    fileEntries.push_back(entry);
                }
            }

            // make sure we account for this.
            totalContentSize += headerSize;

            auto bytesWritten = 0;
            std::ofstream output("H:\\test.out.bin", ios::binary);

            if (output.good())
            {
                auto next2KBoundary = [](unsigned int offset) -> unsigned int {
                    return (offset + 2047) / 2048 * 2048;
                };

                size_t numEntries = fileEntries.size();
                while (numEntries)
                {
                    vector<FileEntry> entries;
                    size_t tmpSize = 4;                 // minimum 4 bytes, since we need to store the number of entries.
                    bytesWritten = 4;

                    size_t processedEntries = 0;

                    // collect upto 1024KB worth of data per chunk.
                    for (auto& e : fileEntries)
                    {
                        size_t new_size = e.data.size() + strlen(e.path.string().c_str()) + 1 + 4;
#ifndef COMPRESS_ALL
                        if (tmpSize + new_size > MAX_CHUNK_SIZE)
                            break;
#endif
                        tmpSize += new_size;                        
                        entries.push_back(e);
                        processedEntries++;
                    }
#ifndef COMPRESS_ALL
                    fileEntries.erase(fileEntries.begin(), fileEntries.begin() + processedEntries);
#endif

                    // construct the chunk
                    char* mainContentChunkBuffer = new char[tmpSize];
                    if (!mainContentChunkBuffer)
                        return;
                    memset(mainContentChunkBuffer, 0, tmpSize);
                    memcpy(mainContentChunkBuffer, &processedEntries, 4);

                    for (auto& e : entries)
                    {
                        const int sz = e.data.size();
                        size_t str_len = strlen(e.path.string().c_str());

                        memcpy((mainContentChunkBuffer + bytesWritten), (void*)&sz, 4);
                        memcpy((mainContentChunkBuffer + bytesWritten + 0x4), (void*)e.path.string().c_str(), str_len);
                        memcpy((mainContentChunkBuffer + bytesWritten + 0x4 + str_len + 1), e.data.data(), sz);

                        bytesWritten += 4 + (str_len + 1) + sz;
                    }

                    // compress the chunk
                    vector<char> mainContentChunk;
                    auto mainContentChunkSize = compressBound(bytesWritten);
                    mainContentChunk.resize(mainContentChunkSize);
                    if (compress2((Bytef*)mainContentChunk.data(), &mainContentChunkSize, (Bytef*)mainContentChunkBuffer, bytesWritten, Z_BEST_COMPRESSION) != Z_OK)
                    {
                        printf("Error: Couldn't compress data\n");
                        return;
                    }

                    // construct the actual chunk, in the proper format
                    output.write((char*)&mainContentChunkSize, 4);
                    output.write((char*)&bytesWritten, 4);

                    int aligned_end = next2KBoundary(output.tellp());
                    while (output.tellp() != aligned_end)
                        output.put(0);
                    output.write(mainContentChunk.data(), mainContentChunkSize);

                    // keep track
                    numEntries -= processedEntries;
                }

                // add the last entry to make the end align to the nearest 0x100
                int lastCompressedBlockSize = 0;
                int lastDecompressedBlockSize = 0;
                output.write((char*)&lastCompressedBlockSize, 4);
                output.write((char*)&lastDecompressedBlockSize, 4);
                size_t aligned_end = next2KBoundary((int)output.tellp());
                while (output.tellp() != aligned_end)
                    output.put(0);

                output.close();
            }
        }
    }

    // read
    Pix(std::ifstream& ifs)
    {
        while (!ifs.eof())
        {
            int compressed_size = 0;
            int decompressed_size = 0;

            ifs.read(reinterpret_cast<char*>(&compressed_size), 4);
            ifs.read(reinterpret_cast<char*>(&decompressed_size), 4);
            
            while (ifs.peek() == 0 || ifs.peek() == 0xDF || ifs.peek() == 0x7F || ifs.peek() == 0xFF ||
                ifs.peek() == 0xBA || ifs.peek() == 0xAB || ifs.peek() == 0xDE || ifs.peek() == 0xFA) 
            {

                ifs.seekg(1, std::ios::cur);
            }

            Entry entry;
            entry.compressed_size = compressed_size;
            entry.decompressed_size = decompressed_size;
            if (compressed_size > 0) {
                entry.data.resize(compressed_size);
                ifs.read((char*)entry.data.data(), compressed_size);
                entries.push_back(entry);
            }
            else 
                break;
        }
        
        int index = 1;
        for (auto& entry : entries)
        {
            std::string name = "entry_" + std::to_string(index) + ".bin";

            uLongf ucompSize = entry.decompressed_size;

            std::vector<unsigned char> uncompressed;
            uncompressed.resize(entry.decompressed_size);

            printf("Processing %s\n", name.c_str());
            uLong unc = entry.data.size();
            int ret = uncompress2((Bytef*)uncompressed.data(), &ucompSize, (Bytef*)entry.data.data(), &unc);
            if (ret == Z_OK) 
            {
                // write out the uncompressed raw chunk
                std::ofstream ofs(name, std::ios::binary);
                if (ofs.good())
                {
                    ofs.write(reinterpret_cast<char*>(uncompressed.data()), uncompressed.size());
                    ofs.close();

                    // process files inside the chunk
                    std::ifstream chunk(name, std::ios::binary);
                    if (chunk.good())
                    {
                        int numberOfFiles = 0;
                        chunk.read(reinterpret_cast<char*>(&numberOfFiles), 4);

                        for (int fileIdx = 0; fileIdx < numberOfFiles; ++fileIdx) 
                        {
                            int size = 0;
                            chunk.read(reinterpret_cast<char*>(&size), 4);

                            std::string fpath;
                            while (chunk.peek() != 0)
                                fpath.push_back(chunk.get());
                            chunk.seekg(1, std::ios::cur);

                            printf("Extracting '%s'...", fpath.c_str());
                            char* buffer = new char[size];
                            memset(buffer, 0, size);
                            chunk.read(buffer, size);

                            std::filesystem::path pth = fpath;
                            if (!std::filesystem::exists(pth.parent_path()))
                                std::filesystem::create_directories(pth.parent_path());

                            std::ofstream file (fpath, std::ios::binary);
                            if (file.good())
                            {
                                file.write(buffer, size);
                                file.close();
                            }

                            delete[] buffer;

                            printf(" Done!\n");
                        }
                    }
                }
            }
            else
            {
                printf("Error processing entry %d! Error: %d\n", index, ret);
                break;
            }
            ++index;
        }
    }
};

int main(int argc, char** argp)
{
    if (argc < 2)
        return -1;

    if (std::string(argp[1]) == "-e" || std::string(argp[1]) == "e")
    {
        std::filesystem::path PIXFile(argp[2]);
        if (!std::filesystem::exists(PIXFile)) 
        {
            printf("Error: Path '%ws' does not exist\n", PIXFile.c_str());
            return -1;
        }

        printf("Processing '%ws'\n", PIXFile.c_str());
        std::ifstream ifs(PIXFile, std::ios::binary);
        if (ifs.good())
        {
            Pix file(ifs);
            ifs.close();
            return 1;
        }
        return 0;
    }
    else if (std::string(argp[1]) == "-nc" || std::string(argp[1]) == "nc")
    {
        if (argc < 4) {
            printf("Usage: pixutils.exe -nc <directory> <output file>\n");
            return -1;
        }

        if (!fs::exists(argp[2])) {
            printf("Error: Input directory does not exist.\n");
            return -1;
        }

        vector<Pix::FileEntry> fileEntries;
        for (auto& p : filesystem::recursive_directory_iterator(argp[2])) {
            if (!p.is_regular_file())
                continue;

            ifstream ifs(p.path(), ios::binary);
            if (ifs.good())
            {
                auto RelPath = fs::relative(p.path(), argp[2]).string();

                printf("Adding '%s'...", RelPath.c_str());
                ifs.seekg(0, ios::end);
                size_t eof = ifs.tellg();
                ifs.seekg(0, ios::beg);

                Pix::FileEntry entry;
                entry.path = RelPath;
                entry.data.resize(eof);
                ifs.read(entry.data.data(), eof);

                printf("(size: 0x%X).\n", (int)entry.data.size());

                fileEntries.push_back(entry);
            }
        }

        int numEntries = fileEntries.size();
        if (numEntries > 0)
        {
            std::ofstream output(argp[3], ios::binary);
            if (output.good())
            {
                output.write((char*)&numEntries, 4);
                for (auto& e : fileEntries)
                {
                    int sz = e.data.size();
                    output.write((char*)&sz, 4);
                    output.write(e.path.string().c_str(), strlen(e.path.string().c_str()));
                    output.put(0);
                    output.write(e.data.data(), e.data.size());
                }
                output.close();

                printf("Written %d entries to %s\n", numEntries, argp[3]);
            }
            else
                printf("Error: Couldn't open output file.\n");
        }
        else
            printf("Error: No files found.\n");
    }
    else if (std::string(argp[1]) == "-c" || std::string(argp[1]) == "c")
    {
        std::filesystem::path dir(argp[2]);
        if (!std::filesystem::exists(dir))
            return -1;

        printf("Creating %s with dir '%ws'\n", std::string(argp[3]).c_str(), dir.c_str());
        Pix file;

        string outputFile = argp[3];
        outputFile.append(".PIX");
        printf("Writing to '%s'\n", outputFile.c_str());
        file.write(argp[3], dir);
        return 1;
    }
    else
    {
        printf("Invalid arguments\n %s", argp[1]);
        return 0;
    }
    return 0;
}