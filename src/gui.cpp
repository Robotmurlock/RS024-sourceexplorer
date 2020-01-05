#include "gui.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <array>
#include "imgui_stdlib.h"
#include <vector>
#include <string>
#include "imgui.h"
#include "TextEditor.h"
#include "keyboard.hpp"

namespace gui {

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

MainWindow::MainWindow() {
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
	exit(EXIT_FAILURE);
    }

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context

    auto monitor = glfwGetPrimaryMonitor();
    auto mode = glfwGetVideoMode(monitor);

    window = glfwCreateWindow(mode->width, mode->height, "CallGraph", NULL, NULL);
    if (window == NULL)
    {
	exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    err = gladLoadGL() == 0;
#else
    err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
	fprintf(stderr, "Failed to initialize OpenGL loader!\n");
	exit(EXIT_FAILURE);
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

MainWindow::~MainWindow() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}


namespace fs = std::filesystem;

void save(const char* new_filename, const std::string& buffer)
{
    std::ofstream outfile(new_filename, std::fstream::out);
    if(!outfile.is_open()) //(#FIXIT) some label better
    {
	std::cerr << "Failed to open file\n";
	exit(1);
    }
    outfile << buffer;

    outfile.close();
}


void FileBrowser::get_directory_files(const fs::path& pathname)
{
	fs::path path(pathname);
	files.clear();

	files.push_back(path.parent_path());
	for(auto& entry_it : fs::directory_iterator(pathname))
	{	
		fs::path entry = fs::canonical(entry_it);
		if(fs::is_directory(entry))
		{
			files.push_back(entry);
		}
		// filter extension .cpp .hpp .h .cc .c
		else
		{
			if(entry.extension() == ".cpp" || entry.extension() == ".hpp" 
			|| entry.extension() == ".h" || entry.extension() == ".cc" || entry.extension() == ".c")
				files.push_back(fs::canonical(entry));
		}
	}

	std::sort(std::begin(files)+1, std::end(files), [](const auto entry1, const auto entry2){
	if(fs::is_directory(entry1))
	{
		if(!fs::is_directory(entry2))
		return true;
		return entry1.compare(entry2) <= 0;
	}
	else if(fs::is_directory(entry2))
	{
		return false;
	}
	else return entry1.compare(entry2) <= 0;

	});

}

void FileBrowser::draw_filebrowser(const char* action, fs::path& filename, bool& write, bool& is_clicked_OPEN)
{		
	ImGui::SetNextWindowSize(ImVec2(500, 400));
	std::string name;

	if(ImGui::Begin(action, &is_clicked_OPEN))
	{
	if(!fs::is_directory(filename))
	{
		new_name = fs::path(filename).filename();
		filename = fs::canonical(filename).parent_path();
	}

	ImGui::Text("[D] %s\n\n", filename.c_str());	
	get_directory_files(filename);

	for(const auto& file : files)
	{		
		if(file == filename.parent_path())
			name = "<= BACK";
		else
			name = file.filename();

		if(ImGui::Selectable(name.c_str()))
		{
			if(name == "<= BACK")
				filename = filename.parent_path();
			else
				filename = file;
			if(fs::is_regular_file(filename))
			{
				new_name = name;
				filename = filename.parent_path();
			}
		}
	}

	ImGui::InputText("###input_filename", &new_name);
	ImGui::SameLine();
	ImGui::Text("(*.cpp, *.hpp, *.h)"); //.cpp, hpp, .h, .cc, .c
	ImGui::Separator();

	if(warning)
	{
		ImGui::TextColored(ImVec4(218.f/255.f, 10.f/255.f, 10.f/255.f, 1.f), "%s", error_msg.c_str());
	}

	if(ImGui::Button("OK"))
	{
		if(fs::is_directory(filename))
		{
			if(new_name == "")
			{
				warning = true;
				error_msg = "Please enter file name\n";
			}
			else
			{				
				filename.append(new_name);
				is_clicked_OPEN = false;
				write = true;
				warning = false;
			}
		}
		else
		{
			warning = true;
			error_msg = "File already exist!";
		}
	}
	ImGui::SameLine();
	if(ImGui::Button("Cancel"))
	{
		is_clicked_OPEN = false;
	}


	ImGui::End();
	}
}


void SourceCodePanel::Draw() {
	
    //*******************
    // KEY EVENTS
    //*******************
    // NEW
    if(io.KeysDown[keyboard::NKey] && io.KeyCtrl)
    {
	is_clicked_NEW = true;
    }
    // OPEN
    if(io.KeysDown[keyboard::OKey] && io.KeyCtrl)
    {
	is_clicked_OPEN = true;
    }
    // SAVE
    if(io.KeysDown[keyboard::SKey] && io.KeyCtrl)
    {
	bt_Save = true;
    }
    // SAVE AS
    if(io.KeysDown[keyboard::SKey] && io.KeyShift && io.KeyCtrl)
    {
	bt_Save = true;
	unsaved = true;
	if(!filename.empty())
	    restore_filename = filename;
	filename.clear();

    }
    // EXIT
    if(io.KeysDown[keyboard::QKey] && io.KeyCtrl)
    {
	glfwSetWindowShouldClose(main_window.Window(), GLFW_TRUE);
    }
    //*******************
    //SOURCE CODE WINDOW
    //*******************
    {

	if(!ImGui::Begin("SOURCE CODE", show_source_code_window, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse))
	{
	    ImGui::End();
	    return;
	}
	
	if(ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
        ImGui::SetWindowFocus(); 

	if(ImGui::BeginMenuBar())
	{
	    if(ImGui::BeginMenu("File"))
	    {
		if(ImGui::MenuItem("New", "Ctrl+N"))
		{
		    is_clicked_NEW = true;
		}
		if(ImGui::MenuItem("Open", "Ctrl+O"))
		{
		    is_clicked_OPEN = true;
		}
		if(ImGui::MenuItem("Save", "Ctrl+S"))
		{
		    bt_Save = true;
		}
		if(ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
		{
		    bt_Save = true;
		    unsaved = true;
		    if(!filename.empty())
				restore_filename = filename;
		    filename.clear();
		}
		if(ImGui::MenuItem("Exit", "Ctrl+Q"))
		{
		    glfwSetWindowShouldClose(main_window.Window(), GLFW_TRUE);
		}
		ImGui::EndMenu();
	    }
	    if(ImGui::BeginMenu("Edit"))
	    {
		if(ImGui::MenuItem("Undo", "Ctrl+Z"))
		{
		    editor.Undo();
		}
		if(ImGui::MenuItem("Redo", "Ctrl+Y"))
		{
		    editor.Redo();
		}
		if(ImGui::MenuItem("Cut", "Ctrl+X"))
		{
		    editor.Cut();
		}
		if(ImGui::MenuItem("Copy", "Ctrl+C"))
		{
		    editor.Copy();
		}
		if(ImGui::MenuItem("Paste", "Ctrl+V"))
		{
		    editor.Paste();
		}

		ImGui::EndMenu();
	    }

	    ImGui::EndMenuBar();
	}

	// Writing file name instead of absoulte path	
	ImGui::Text("%s", filename.filename().c_str());
	ImGui::SameLine();

	if(!unsaved)
	    unsaved = editor.IsTextChanged();

	ImGui::Text("%s", (unsaved ? "*" : ""));

	editor.Render("Source Code Editor");

	ImGui::End();
    }
    //*******************
    //NEW BUTTON WINDOW
    //*******************
	if(file == "" || !fs::is_directory(file))
		file = fs::current_path();

    if(is_clicked_NEW)
    {
		//file = fs::current_path();
		file = fs::canonical(file);
		static bool write = false;

		file_browser.draw_filebrowser("NEW", file, write, is_clicked_NEW);
		if(write)
		{
			std::cout << file << std::endl;
			filename = file;
			std::ofstream output_stream(filename);
			if(!output_stream.is_open())
			{
			std::cerr << "Failed to create file\n";
			exit(1);
			}

			output_stream << editor.GetText();

			output_stream.close();
			is_clicked_NEW = false;
			write = false;
			file = fs::current_path();
		}
    }

    //*******************
    //OPEN BUTTON WINDOW
    //*******************	
    if(is_clicked_OPEN)
    {
		static bool write = false;
		file = fs::canonical(file);

		file_browser.draw_filebrowser("OPEN", file, write, is_clicked_OPEN); //  editor_util/editor_util.hpp
		if(write && fs::is_regular_file(file))
		{
			filename = fs::canonical(file);
			std::ifstream in_file(filename);
			std::string _str;
			directory_of_last_opened_file = fs::absolute(file).remove_filename();

			std::string buffer = "";

			while(std::getline(in_file, _str))
			{
				buffer.append(_str);
				buffer.append("\n");
			}

			//graph.BuildCallgraphFromSource(buffer);
			should_build_callgraph = true;
			editor.SetText(buffer);
			
			write = false;
			file = fs::current_path();
		}
    }

    //*******************
    //SAVE BUTTON WINDOW
    //*******************
    static bool save_prompt = false;    
    static bool write = false;
    if(bt_Save)
    {
	file = fs::canonical(file);
	if(!unsaved) //IGNORE SAVE EVENT
	{}
	else if(!filename.empty())
	{
	    save(filename.c_str(), editor.GetText());
	    bt_Save = false;
	    unsaved = false;
	}
	else
	{
	    file_browser.draw_filebrowser("SAVE", file, write, bt_Save);
	    if(write && (!fs::exists(file) || fs::is_regular_file(file)))
	    {
		if(!fs::exists(file))
		{
		    filename = file;

		    std::ofstream output_stream(filename);
		    if(!output_stream.is_open())
		    {
			std::cerr << "Failed to create file\n";
			exit(1);
		    }

		    //output_stream << editor.GetText();

		    save(filename.c_str(), editor.GetText());

		    output_stream.close();
		    unsaved = false;
		    write = false;
		}
		else if(fs::is_empty(file))
		{
		    filename = file;
		    save(filename.c_str(), editor.GetText());
		    unsaved = false;
		    write = false;
		}
		else
		{
		    save_prompt = true;
		}
	    }
	    if(!bt_Save && filename == "")
		filename = restore_filename;
	}
    }

    if(save_prompt)
    {
	ImGui::SetNextWindowSize(ImVec2(200, 90));
	if(ImGui::Begin("###save_prompt", &save_prompt))
	{

	    ImGui::Text("Do you want to overwrite?");

	    if(ImGui::Button("OK"))
	    {
		save_prompt = false;
		bt_Save = false;
		filename = file;
		save(filename.c_str(), editor.GetText());
		file = ".";
		unsaved = false;
		write = false;
	    }
	    ImGui::SameLine();
	    if(ImGui::Button("Cancel"))
	    {
		save_prompt = false;
		bt_Save = false;
		file = ".";
		write = false;
	    }

	    ImGui::End();
	}
    }

    if(editor.IsTextChanged()) {
	should_build_callgraph = true;
    }
}



void WindowsToggleMenu::Draw() {
    ImGui::Begin("Windows toggle menu", __null, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	if(ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
        ImGui::SetWindowFocus(); 
    ImGui::Checkbox("Source code", &show_source_code_window); ImGui::SameLine(150);
    ImGui::Checkbox("Callgraph", &show_callgraph_window); ImGui::SameLine(300);
    ImGui::Checkbox("AST dump", &show_ast_dump_window); ImGui::SameLine(450);
    ImGui::Checkbox("Function list", &show_function_list_window); ImGui::SameLine(600);

    ImGui::End();
}

void FunctionListFilteringWindow::Draw() {
    ImGui::Begin("Functions Filtering List", &p_open, ImGuiWindowFlags_NoCollapse);
	if(ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
        ImGui::SetWindowFocus(); 
    ImGui::Text("Filter usage:\n"
		"  \"\"         display all lines\n"
		"  \"xxx\"      display lines containing \"xxx\"\n"
		"  \"xxx,yyy\"  display lines containing \"xxx\" or \"yyy\"\n"
		"  \"-xxx\"     hide lines containing \"xxx\"");
    filter.Draw();

    if(functions) {
	for(const auto& function : *functions) {
	    if(filter.PassFilter(function->NameAsString().c_str())) {
		bool open = ImGui::TreeNode(function->NameAsString().c_str());
		bool clicked = ImGui::IsItemClicked();

		if(open)
		{
		    ImGui::Text("Return type: %s", function->ReturnTypeAsString().c_str());
		    if(function->HasParams()) {
			ImGui::Text("Params: ");
			for(auto param = function->ParamBegin(); param != function->ParamEnd(); ++param)
			{
			    ImGui::Text("\t%s %s", param->TypeAsString().c_str(), param->NameAsString().c_str());
			}

		    } else {
			ImGui::Text("Params: None");
		    }
		    ImGui::TreePop();
		}
		if(clicked)
		{
		    last_clicked = function.get();
		}
	    }
	}
    }

    ImGui::End();
}



void FunctionASTDumpWindow::Draw() {
    ImGui::Begin("Function AST Dump", &p_open, ImGuiWindowFlags_HorizontalScrollbar);
	if(ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
        ImGui::SetWindowFocus(); 
    if(function)
    {
	ImGui::Text("%s", function->ASTDump().c_str());
    }
    else
    {
	ImGui::Text("None");
    }
    ImGui::End();
}



};
