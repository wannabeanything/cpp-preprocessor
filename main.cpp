#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using std::filesystem::path;
using namespace std::literals::string_literals;

path operator""_p (const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void PrintNegative (const std::string &include_filename,
                    const std::string &parent_filename,
                    size_t line_no) {
    std::cout << "unknown include file " << include_filename << " at file "
              << parent_filename << " at line " << line_no << std::endl;
}

bool PreprocessInternal (std::ifstream& input_stream,
                     std::ofstream& output_stream,
                     const path& in_file,
                     const std::vector<path>& include_directories) {
    static std::regex single_include(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static std::regex multy_include(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    std::smatch match;
    std::string read_line;
    size_t line_no = 0;

    while (std::getline(input_stream, read_line)) {
        path file_path;
        bool found = true;
        ++ line_no;

        if (std::regex_match(read_line, match, single_include)) {
            file_path = in_file.parent_path() / std::string(match[1]);

            if (std::filesystem::exists(file_path)) {
                std::ifstream new_input_stream(file_path.string(), std::ios::in);
                if (new_input_stream.is_open()) {
                    if (!PreprocessInternal(new_input_stream,
                                        output_stream,
                                        file_path.string(),
                                        include_directories)) {
                        return false;
                    }
                    continue;
                } else {
                    PrintNegative(file_path.filename().string(),
                                  in_file.string(),
                                  line_no);
                    return false;
                }
            } else {
                found = false;
            }
        }

        if ((!found) || (std::regex_match(read_line, match, multy_include))) {
            bool founded = false;
            for (const auto& dir : include_directories) {
                file_path = dir / std::string(match[1]);
                if (std::filesystem::exists(file_path)) {
                    std::ifstream new_input_stream(file_path.string(), std::ios::in);
                    if (new_input_stream.is_open()) {
                        if (!PreprocessInternal(new_input_stream,
                                            output_stream,
                                            file_path.string(),
                                            include_directories)) {
                            return false;
                        }
                        founded = true;
                        break;
                    } else {
                        PrintNegative(file_path.filename().string(),
                                      in_file.string(),
                                      line_no);
                        return false;
                    }
                }
            }
            if (!founded) {
                PrintNegative(file_path.filename().string(),
                              in_file.string(),
                              line_no);
                return false;
            }
            continue;
        }
        output_stream << read_line << std::endl;
    }
    return true;
}

bool Preprocess (const path& in_file,
                 const path& out_file,
                 const std::vector<path>& include_directories) {
    if (!std::filesystem::exists(in_file)) {
        return false;
    }
    if (std::filesystem::is_empty(in_file)) {
        return false;
    }

    std::ifstream input_stream(in_file);
    if (!input_stream) {
        return false;
    }

    std::ofstream output_stream(out_file);
    if (!output_stream) {
        return false;
    }
    
    return PreprocessInternal(input_stream, output_stream, in_file, include_directories);
}

std::string GetFileContents(std::string file) {
    std::ifstream stream(file);

    return {(std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>()};
}

void Test() {
    std::error_code err;
    std::filesystem::remove_all("sources"_p, err);
    std::filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    std::filesystem::create_directories("sources"_p / "include1"_p, err);
    std::filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        std::ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        std::ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        std::ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        std::ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        std::ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        std::ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    std::ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
    return 0;
}