#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <stack>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}


//We need to go through file and there are 3 cases. 
//1 case: include "..." -- we look through the folder in which the file is and look for ...
//If not found -- check vector
//2 case: include <...> -- we just look through the vector
//If we found the file we paste the contents of it 
//and if there's another file we do the same and so on
//3 case: just a line that we we write


void TextError(const path& searched_file, const path& src, int line){
    cout << "unknown include file " << searched_file.string()
    << " at file " << src.string() << " at line " << line << '\n';
}

// Maybe we should treat this this way: 
//a simple line(not include) is a base case
//everything else is going deeper unless
//it reaches a plain text 
bool PreproccessInternal(ifstream& file_to_read, ofstream& file_to_write, const path& in_file, const vector<path>& include_directories);
bool PreproccessFolder(const path& in_file, const path& file_to_find, 
                        ofstream& file_to_write, int line_of_code, const vector<path>& include_directories){
    bool found = false;
    for(const auto& entry: filesystem::recursive_directory_iterator(in_file.parent_path())){
        if(filesystem::is_regular_file(entry) && entry == file_to_find){
            found = true;
            break;
        }
    }
    ifstream new_file_to_read(file_to_find);
    if(!new_file_to_read)return false;
    if(!PreproccessInternal(new_file_to_read, file_to_write, file_to_find, include_directories) && !found){
        TextError(file_to_find, in_file, line_of_code);
        return false;
    }
    
    return true;
    
}


bool PreproccessInternal(ifstream& file_to_read, ofstream& file_to_write, const path& in_file, const vector<path>& include_directories){
    // static regex include_file_pattern(R"(\s*#\s*include\s*\"([^"]*)\"\s*)");// is for #include "..."
    // static regex include_lib_pattern(R"(\s*#\s*include\s*<([^>]*)>\s*)"); // is for #include <...>
    
    static std::regex include_pattern(R"/(\s*#\s*include\s*["<]([^">]*)[">]\s*)/"); // universal
    smatch match;
    string line;
    int line_of_code = 0;
    path file_to_find;
    while(getline(file_to_read, line)){
        line_of_code++;
        if(regex_match(line, match, include_pattern)){
            file_to_find = in_file.parent_path()/match[1].str();
        
            if(!filesystem::exists(file_to_find)){
                bool found = false;
                for(const auto& dir: include_directories){
                    file_to_find = dir / match[1].str();
                    
                    if(filesystem::exists(file_to_find)){
                        found = true;
                        break;
                    }
                }
                if(!found){
                    TextError(match[1].str(), in_file, line_of_code);
                    return false;
                }
            }
            if(!PreproccessFolder(in_file, file_to_find, file_to_write, line_of_code, include_directories))return false;
            continue;
        }
        file_to_write << line << '\n';
    }
    return true;
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories){
    ifstream file_read(in_file);
    if(!file_read)return false;

    ofstream file_write(out_file);
    if(!file_write)return false;

    return PreproccessInternal(file_read, file_write, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
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
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }
    //Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                //   {"sources"_p / "include1"_p,"sources"_p / "include2"_p});
    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
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
}