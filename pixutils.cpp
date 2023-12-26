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

    void create(std::string levelName, std::filesystem::path path)
    {

        fs::path rtdDir = path;
        rtdDir.append("rtd/");

        fs::path levelFile = rtdDir;
        levelFile.append(levelName + ".drl");

        if (!fs::exists(rtdDir) || !fs::exists(levelFile))
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
                if (!p.is_regular_file() || p == levelFile)
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

            totalContentSize += headerSize;

            char* mainContentChunkBuffer = new char[totalContentSize];
            if (!mainContentChunkBuffer)
                return;

            memset(mainContentChunkBuffer, 0, totalContentSize);
            auto bytesWritten = 0;
            for (const auto& e : fileEntries)
            {
                const int sz = e.data.size();
                size_t str_len = strlen(e.path.string().c_str());

                memcpy((mainContentChunkBuffer + bytesWritten), (void*)&sz, 4);
                memcpy((mainContentChunkBuffer + bytesWritten + 0x4), (void*)e.path.c_str(), str_len);
                memset((mainContentChunkBuffer + bytesWritten + 0x4 + str_len), 0, 1);
                memcpy((mainContentChunkBuffer + bytesWritten + 0x4 + str_len + 1), e.data.data(), sz);

                bytesWritten += 4 + (str_len + 1) + (int)sz;
            }

            // compress the whole block
            vector<char> mainContentChunk;

            auto mainContentChunkSize = compressBound(bytesWritten);
            mainContentChunk.resize(mainContentChunkSize);
            compress2((Bytef*)mainContentChunk.data(), &mainContentChunkSize, (Bytef*)mainContentChunkBuffer, bytesWritten, Z_BEST_COMPRESSION);


            // do the same, but just for the level file and make sure we process that at the end.
            FileEntry LevelFileEntry;

            std::ifstream levelIfs(levelFile, std::ios::binary);
            if (!levelIfs.good())
                return;

            LevelFileEntry.path = levelFile;
            levelIfs.seekg(0, ios::end);
            LevelFileEntry.data.resize(levelIfs.tellg());
            levelIfs.seekg(0, ios::beg);

            levelIfs.read(LevelFileEntry.data.data(), LevelFileEntry.data.size());
            levelIfs.close();

            size_t levelChunkRawSize = 4 + LevelFileEntry.path.string().size() + LevelFileEntry.data.size();
            char* levelEntry = new char[levelChunkRawSize];
            if (!levelEntry)
                return;

            int nIdx = 1;
            memcpy(levelEntry, &nIdx, 4);
            std::string RelPath = fs::relative(LevelFileEntry.path, path).string();
            auto str_len = strlen(RelPath.c_str());
            memcpy(levelEntry + 0x4, (void*)RelPath.c_str(), str_len);
            memset(levelEntry + 0x4 + str_len, 0, 1);
            memcpy(levelEntry + 0x4 + str_len + 1, LevelFileEntry.data.data(), LevelFileEntry.data.size());


            // compress the whole block
            auto levelChunkCompressedSize = compressBound(levelChunkRawSize);

            vector<char> levelChunk;
            levelChunk.resize(levelChunkCompressedSize);
            compress2((Bytef*)levelChunk.data(), &levelChunkCompressedSize, (Bytef*)mainContentChunkBuffer, levelChunkRawSize, Z_BEST_COMPRESSION);

            std::ofstream output("H:\\test.out.bin", ios::binary);
            if (output.good())
            {
                // write out main content
                output.write((char*)&totalContentSize, 4);
                output.write((char*)&mainContentChunkSize, 4);
                for (size_t i = 0; i < 0x7F8; ++i)
                    output.put(0);
                output.write(mainContentChunk.data(), mainContentChunkSize);

                // then the level chunk with just the .drl
                output.write((char*)&levelChunkRawSize, 4);
                output.write((char*)&levelChunkCompressedSize, 4);
                
                size_t s = (((int)output.tellp()) + 2048 - 1) & ~(2048 - 1);
                printf("%x\n", (int)s);
                while (output.tellp() != s)
                    output.put(0);


                output.write(levelChunk.data(), levelChunkCompressedSize);

                // add the last entry to make the end align to the nearest 0x100
                int lastCompressedBlockSize = 0;
                int lastDecompressedBlockSize = 0;
                output.write((char*)&lastCompressedBlockSize, 4);
                output.write((char*)&lastDecompressedBlockSize, 4);
                while (!((int)output.tellp() & 0x100))
                    output.put(0);

                output.close();
            }
        }
    }


    Pix(std::string levelName, std::filesystem::path path)
    {
        entries.clear();
    }
    void write(std::filesystem::path path)
    {

    }


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
            Z_MEM_ERROR;
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
    else if (std::string(argp[1]) == "-c" || std::string(argp[1]) == "c")
    {
        std::filesystem::path dir(argp[2]);
        if (!std::filesystem::exists(dir))
            return -1;

        printf("Creating %s with dir '%ws'\n", std::string(argp[3]).c_str(), dir.c_str());
        Pix file(argp[3], dir);
        file.create(argp[3], dir);

        string outputFile = argp[3];
        outputFile.append(".PIX");
        printf("Writing to '%s'\n", outputFile.c_str());
        file.write(outputFile);
        return 1;
    }
    else
    {
        printf("Invalid arguments\n %s", argp[1]);
        return 0;
    }
    return 0;
}