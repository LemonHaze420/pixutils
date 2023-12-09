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

class Pix {
public:

    struct Entry {
        int compressed_size = 0;
        int decompressed_size = 0;

        vector<unsigned char> data;
    };    

    vector<Entry> entries;

    Pix(std::ifstream& ifs)
    {
        while (!ifs.eof()) 
        {
            int compressed_size = 0;
            int decompressed_size = 0;

            ifs.read(reinterpret_cast<char*>(&compressed_size), 4);
            ifs.read(reinterpret_cast<char*>(&decompressed_size), 4);
            while (ifs.peek() == 0)
                ifs.seekg(1, std::ios::cur);

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

            int ret = 0;
            if (ret = uncompress((Bytef*)uncompressed.data(), &ucompSize, (Bytef*)entry.data.data(), (uLong)entry.data.size()) == Z_OK) 
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

                        for (int i = 0; i < numberOfFiles; ++i) 
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
    if (argc < 1)
    {
        return -1;
    }


    std::filesystem::path PIXFile(argp[1]);
    if (!std::filesystem::exists(PIXFile))
        return -1;

    printf("Processing '%ws'\n", PIXFile.c_str());
    std::ifstream ifs(PIXFile, std::ios::binary);
    if (ifs.good())
    {
        Pix file(ifs);

        ifs.close();
    }
    else
        return -1;

    return 1;
}