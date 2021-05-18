#include "../include/gui.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif
constexpr int MAX_LEN = 256;
constexpr int MAX_CLOCK = 60 * 60 * 24;

static const char* xlabels[MAX_LEN];
static const char* plabels[MAX_LEN];
static const char* tlabels[MAX_CLOCK];
static char buf_fstree[FS::BLK_SIZE * FS::MAX_N_BLKS];
static char cd_buf[FS::MAX_NAME_LEN];

static int pname_filter(ImGuiInputTextCallbackData* data) {
    if (data->EventChar < 255 && data->EventChar != '/')
        return 0;
    return 1;
}

struct file {
    string fname;
    int fsize;
};

struct dir {
    string dirname;

};

static struct TreeItem* tree_root = new TreeItem("/", 0, 1, nullptr);
static struct TreeItem* now_tree_node;

void ls_root(Kernel* kernel, string path) {
    struct FS::Inode* inode = new struct FS::Inode;
    struct FS::Dir* dir = new struct FS::Dir[8];
    int idx = kernel->fs->walk(path, inode, dir);
    for (int i = 0; i < 8; i++) {
        if (dir[i].type == FS::File_t::None) continue; 
        string name{ dir[i].entry_name };
        if (dir[i].type == FS::File_t::Dir) {
            if (name == ".."
                || name == ".") continue;
            now_tree_node->add(name, 0, 1);
            now_tree_node = now_tree_node->lastChild();
            string npath = path[path.size() - 1] == '/' ?
                path + name : path + "/" + name;
            ls_root(kernel, npath);
            now_tree_node = now_tree_node->parent;
        }
        if (dir[i].type == FS::File_t::File) {
            FS::read_inode(inode, dir[i].inode);
            now_tree_node->add(name, inode->i_size, 0);
        }
    }
    delete inode;
    delete[] dir;
}
static int litem = 0;
void rec_tree(struct TreeItem* root) {
    if (root) {
        if (root->type == 0) {
            ImGui::BulletText("%s Size=%d Bytes",
                now_tree_node->name.c_str(), now_tree_node->size);
            rec_tree(root->next_sibling);
        }
        else if (root->type == 1) {
            ImGui::TreeNodeEx((void*)(intptr_t)litem++, ImGuiTreeNodeFlags_None, "%s", root->name.c_str());
            rec_tree(root->firstchild);
            ImGui::TreePop();
            rec_tree(root->next_sibling);
        }
    }
}

static bool edit = false;
static bool file_editor = false;
static string fs_node_full_path{};
static bool is_open_fstree = true;
static bool mem_set = false;

void show_file_viewer(Kernel* kernel, bool* popen) {
    static char* buf_fstree = new char[FS::BLK_SIZE * FS::MAX_N_BLKS];
    memset(buf_fstree, 0, FS::BLK_SIZE * FS::MAX_N_BLKS);
    kernel->fs->read(fs_node_full_path, buf_fstree, 0, -1);

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("File Viewer", popen, status_flags);
    {
        ImGui::Text("%s", fs_node_full_path);
        ImGui::Separator();
        
        if (ImGui::Button("Edit File")) {
            edit = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (edit) {
                kernel->fs->write(fs_node_full_path,
                    buf_fstree, 0, strlen(buf_fstree));
            }
        }
        ImGui::Separator();
        ImGui::InputTextMultiline(fs_node_full_path.c_str(),
            buf_fstree, FS::BLK_SIZE * FS::MAX_N_BLKS,
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
            edit ? ImGuiInputTextFlags_AllowTabInput :
            ImGuiInputTextFlags_ReadOnly);
        delete[] buf_fstree;
    }
    ImGui::End();
}
    

static vector<const char*> states = {
        "Newborn",
        "Ready",
        "Running",
        "Waiting",
        "Dead"
};

static map<string, int> state_map = {
    {"Newborn", 0},
    {"Ready", 1},
    {"Running", 2},
    {"Waiting", 3},
    {"Dead", 4}
};

static ImVec4 g_color_map[5] = {
    ImVec4(0.0f, 0.0f, 0.0f, 1.0f),
    ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
    ImVec4(0.0f, 0.0f, 1.0f, 1.0f),
    ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
    ImVec4(0.0f, 0.0f, 0.0f, 1.0f),
};

static int kernel_clock = -1;
static int x_clock = -1;
static int pid_hovered = -1;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

struct g_entry {
    int clock;
    int pid;
    string pname;
    int pstate;
};

static map<int, vector<g_entry>> g_history;
static string pwd;

void make_stamp(vector<vector<string>> ps) {
    for (auto k : ps) {
        if (k[1] == "init") continue;
        struct g_entry e;
        e.clock = kernel_clock;
        e.pid = stoi(k[0]);
        e.pname = k[1];
        e.pstate = state_map.at(k[2]);
        g_history[stoi(k[0])].push_back(e);
    }
}

static pair<string, string> alg;
const char* pas[5] = {
    "FCFS",
    "SJF",
    "RR",
    "PR",
    "MQ"
};
const char* mas[2] = {
    "FIFO",
    "LRU"
};
const char* dummy_labels[52] = {
    "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P",
    "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "a", "b", "c", "d", "e", "f", "g", "h",
    "i", "j", "k", "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
};
static int palg;
static int malg;

void main_window(Kernel* kernel) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return;
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    GLFWwindow* window = glfwCreateWindow(1200, 800, "System Status", NULL, NULL);
    if (window == NULL)
        return;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool err = false;
    glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool err = false;
    glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)glfwGetProcAddress(name); });
#else
    bool err = false;
#endif
    if (err) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImFont* consolas = io.Fonts->AddFontFromFileTTF("./consola.ttf", 16.0f);
    
    if (consolas) io.Fonts->Build();
    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    vector<vector<string>> process_states = kernel->expose_pr();
    vector<vector<string>> mem_states_per_proc = kernel->mem_pr();
    vector<int> mem_map = kernel->mem_map();
    int map_len = min(MAX_LEN, static_cast<int>(ceil(sqrt(mem_map.size()))));
    for (int i = 0; i < MAX_LEN; i++) {
        xlabels[i] = "";
    }
    static int mmap[MAX_LEN * MAX_LEN];
    vector<int> swap_blks = kernel->swap_map();

    ImVec2 clk_sz;
    clk_sz.x = 52.0; clk_sz.y = 52.0;
    ImVec2 clk_pos;
    clk_pos.x = 1134.0; clk_pos.y = 0.0;
    ImVec2 ps_sz;
    ps_sz.x = 550.0; ps_sz.y = 440.0;
    ImVec2 ps_pos;
    ps_pos.x = 4.0; ps_pos.y = 4.0;
    ImVec2 mem_sz;
    mem_sz.x = 447.0; mem_sz.y = 440.0;
    ImVec2 mem_pos;
    mem_pos.x = 4.0; mem_pos.y = 358.0;

    static double avgturnaround = 0;

    while (!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (!kernel) break;
        glfwPollEvents();
        int kcl = kernel->get_clock();
        if (x_clock == -1) x_clock = kcl;
        if (malg >= 0 && palg >= 0) {
            alg.first = string(pas[palg]);
            alg.second = string(mas[malg]);
        }
        if (kcl > kernel_clock) {
            kernel_clock = kcl;
            char* tick = new char[MAX_LEN];
            strcpy(tick, to_string(kernel_clock).c_str());
            tlabels[kernel_clock - x_clock] = tick;
            process_states = kernel->expose_pr();
            make_stamp(process_states);
            int p = 0;
            for (auto& [k, v] : g_history) {
                plabels[p++] = v[0].pname.c_str();
            }
            mem_states_per_proc = kernel->mem_pr();
            mem_map = kernel->mem_map();
            swap_blks = kernel->swap_map();
            avgturnaround = kernel->statistic();
            alg = kernel->alg();
            if (palg >= 0 && malg >= 0 &&
                (strcmp(pas[palg], alg.first.c_str())
                || strcmp(mas[malg], alg.second.c_str()))) {
                PR::Algorithm paa;
                MM::Algorithm maa;
                switch (palg) {
                case 0:
                    paa = PR::Algorithm::FCFS; break;
                case 1:
                    paa = PR::Algorithm::SJF; break;
                case 2:
                    paa = PR::Algorithm::RR; break;
                case 3:
                    paa = PR::Algorithm::PRIORITY; break;
                case 4:
                    paa = PR::Algorithm::MIXED_QUEUE; break;
                default:
                    paa = PR::Algorithm::NONE; break;
                }
                switch (malg) {
                case 0:
                    maa = MM::Algorithm::FIFO; break;
                case 1:
                    maa = MM::Algorithm::LRU; break;
                default:
                    maa = MM::Algorithm::NONE; break;
                }
                kernel->chalg(paa, maa);
            }
            pwd = kernel->get_pwd();
            delete tree_root;
            tree_root = new TreeItem("/", 0, 1, nullptr);
            now_tree_node = tree_root;
            ls_root(kernel, "/");
        }

        for (int i = 0; i < map_len; i++) {
            for (int j = 0; j < map_len; j++) {
                try {
                    auto v = mem_map.at(i * map_len + j);
                    mmap[i * map_len + j] = v;
                }
                catch (...) {
                    mmap[i * map_len + j] = -1;
                }
            }
        }

        /*if (!file_editor) {
            mem_set = false;
            edit = false;
        }*/

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            ImGui::Begin("Clock", 0, clk_flags);
            ImGui::Text("%d", kernel_clock);
            ImGui::End();
            if (kernel_clock >= 0) {
                {
                    ImGui::Begin("Toolbox", 0, status_flags);
                    if (ImGui::CollapsingHeader("System Info", ImGuiTreeNodeFlags_None)) {
                        ImGui::Text("System Average Turnaround Time: %.2f", avgturnaround);
                        ImGui::Separator();
                        ImGui::Text("Process Scheduler: %s", alg.first.c_str());
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(100.0f);
                        ImGui::Combo(" ", &palg, pas, 5);
                        ImGui::Text("Memory Allocator: %s", alg.second.c_str());
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(100.0f);
                        ImGui::Combo("  ", &malg, mas, 2);
                    }
                    if (ImGui::CollapsingHeader("New...", ImGuiTreeNodeFlags_None))
                    {
                        if (ImGui::BeginTabBar("Toolbox_tabs", tabs_flags))
                        {
                            if (ImGui::BeginTabItem("Process"))
                            {
                                static char pname[FS::MAX_NAME_LEN] = "";
                                static char ppath[FS::MAX_NAME_LEN] = "";
                                static char code[FS::BLK_SIZE] = "";
                                ImGui::Text("pwd: %s", pwd.c_str());
                                ImGui::Separator();
                                ImGui::InputTextWithHint(" ", "Process Path(Blank=pwd)",
                                    ppath, FS::MAX_NAME_LEN);
                                ImGui::InputTextWithHint("  ", "Process Name",
                                    pname, FS::MAX_NAME_LEN,
                                    ImGuiInputTextFlags_CallbackCharFilter,
                                    pname_filter);
                                ImGui::InputTextMultiline("   ", code,
                                    FS::BLK_SIZE, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                                    ImGuiInputTextFlags_AllowTabInput);
                                if (ImGui::Button("Submit")) {
                                    string path = string(ppath);
                                    if (path.size() && path[path.size() - 1] == '/')
                                        path = path.substr(0, path.size() - 1);
                                    string name = string(pname);
                                    bool ok = kernel->fs->create(path + "/", name, FS::File_t::File);
                                    if (ok) {
                                        kernel->fs->write(path + "/" + name, code, 0, strlen(code));
                                        kernel->sch->exec(path + "/" + name, kernel->sch->fork(1));
                                    }
                                    memset(pname, 0, FS::MAX_NAME_LEN);
                                    memset(ppath, 0, FS::MAX_NAME_LEN);
                                    memset(code, 0, FS::BLK_SIZE);
                                    delete tree_root;
                                    tree_root = new TreeItem("/", 0, 1, nullptr);
                                    now_tree_node = tree_root;
                                    ls_root(kernel, "/");
                                }
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("File"))
                            {
                                static char fname[FS::MAX_NAME_LEN] = "";
                                static char fpath[FS::MAX_NAME_LEN] = "";
                                static char fcont[FS::BLK_SIZE] = "";
                                ImGui::Text("pwd: %s", pwd.c_str());
                                ImGui::Separator();
                                ImGui::InputTextWithHint(" ", "File Path(Blank=pwd)",
                                    fpath, FS::MAX_NAME_LEN);
                                ImGui::InputTextWithHint("  ", "File Name",
                                    fname, FS::MAX_NAME_LEN,
                                    ImGuiInputTextFlags_CallbackCharFilter,
                                    pname_filter);
                                ImGui::InputTextMultiline("   ", fcont,
                                    FS::BLK_SIZE, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                                    ImGuiInputTextFlags_AllowTabInput);
                                if (ImGui::Button("Submit")) {
                                    string path = string(fpath);
                                    if (path.size() && path[path.size() - 1] == '/')
                                        path = path.substr(0, path.size() - 1);
                                    string name = string(fname);
                                    kernel->fs->create(path + "/", name, FS::File_t::File);
                                    kernel->fs->write(path + "/" + name, fcont, 0, strlen(fcont));
                                    memset(fname, 0, FS::MAX_NAME_LEN);
                                    memset(fpath, 0, FS::MAX_NAME_LEN);
                                    memset(fcont, 0, FS::BLK_SIZE);
                                    delete tree_root;
                                    tree_root = new TreeItem("/", 0, 1, nullptr);
                                    now_tree_node = tree_root;
                                    ls_root(kernel, "/");
                                }
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Directory"))
                            {
                                static char dname[FS::MAX_NAME_LEN] = "";
                                static char dpath[FS::MAX_NAME_LEN] = "";
                                ImGui::Text("pwd: %s", pwd.c_str());
                                ImGui::Separator();
                                ImGui::InputTextWithHint(" ", "Directory Path(Blank=pwd)",
                                    dpath, FS::MAX_NAME_LEN);
                                ImGui::InputTextWithHint("  ", "Directory Name",
                                    dname, FS::MAX_NAME_LEN,
                                    ImGuiInputTextFlags_CallbackCharFilter,
                                    pname_filter);
                                if (ImGui::Button("Submit")) {
                                    string path = string(dpath);
                                    if (path.size() && path[path.size() - 1] == '/')
                                        path = path.substr(0, path.size() - 1);
                                    string name = string(dname);
                                    kernel->fs->create(path + "/", name, FS::File_t::Dir);
                                    memset(dname, 0, FS::MAX_NAME_LEN);
                                    memset(dpath, 0, FS::MAX_NAME_LEN);
                                    delete tree_root;
                                    tree_root = new TreeItem("/", 0, 1, nullptr);
                                    now_tree_node = tree_root;
                                    ls_root(kernel, "/");
                                }
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Swapspace"))
                            {
                                static char sname[FS::MAX_NAME_LEN] = "";
                                static char spath[FS::MAX_NAME_LEN] = "";
                                ImGui::Text("pwd: %s", pwd.c_str());
                                ImGui::Separator();
                                ImGui::InputTextWithHint(" ", "Swapfile Path(Blank=pwd)",
                                    spath, FS::MAX_NAME_LEN);
                                ImGui::InputTextWithHint("  ", "Swapfile Name",
                                    sname, FS::MAX_NAME_LEN,
                                    ImGuiInputTextFlags_CallbackCharFilter,
                                    pname_filter);
                                if (ImGui::Button("Submit")) {
                                    string path = string(spath);
                                    if (path.size() && path[path.size() - 1] == '/')
                                        path = path.substr(0, path.size() - 1);
                                    string name = string(sname);
                                    kernel->pg->new_swap(path + "/" + name);
                                    kernel->fs->create_swapspace(path + "/", name);
                                    memset(sname, 0, FS::MAX_NAME_LEN);
                                    memset(spath, 0, FS::MAX_NAME_LEN);
                                    delete tree_root;
                                    tree_root = new TreeItem("/", 0, 1, nullptr);
                                    now_tree_node = tree_root;
                                    ls_root(kernel, "/");
                                }
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                    }
                    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_None)) {

                    }
                    if (ImGui::CollapsingHeader("File System", ImGuiTreeNodeFlags_None)) {
                        ImGui::InputTextWithHint("##cd", "Change Directory", cd_buf,
                            FS::MAX_NAME_LEN);
                        ImGui::SameLine();
                        if(ImGui::Button("Set"))
                        {
                            string path = string(cd_buf);
                            if (path.size()) {
                                kernel->fs->set_pwd(path);
                                pwd = kernel->fs->get_pwd();
                            }
                        }
                        ImGui::Text("pwd: %s", pwd.c_str());
                        litem = 0;
                        int dummy_index = 0;
                        now_tree_node = tree_root;
                        stack<TreeItem*> stk;
                        bool drawing = true;
                        while (now_tree_node || stk.size()) {
                            while (now_tree_node)
                            {
                                if (now_tree_node->type == 0) {
                                    ImGui::BulletText("%s Size=%d Bytes",
                                        now_tree_node->name.c_str(), now_tree_node->size);
                                    if (ImGui::BeginPopupContextItem(dummy_labels[dummy_index++]))
                                    {
                                        if (ImGui::Selectable("Open")) {
                                            fs_node_full_path = now_tree_node->cat();
                                            file_editor = true;
                                            mem_set = false;
                                            edit = false;
                                        }
                                        if (ImGui::Selectable("Delete")) {
                                            kernel->fs->fdelete(now_tree_node->cat());
                                        }
                                        if (ImGui::Selectable("Execute")) {
                                            kernel->sch->exec(now_tree_node->cat(), 
                                                kernel->sch->fork(1));
                                        }
                                        ImGui::EndPopup();
                                    }
                                }
                                else {
                                    if (!ImGui::TreeNodeEx((void*)(intptr_t)litem++,
                                        ImGuiTreeNodeFlags_None,
                                        "%s", now_tree_node->name.c_str())) {
                                        if (ImGui::BeginPopupContextItem(dummy_labels[dummy_index++]))
                                        {
                                            if (ImGui::Selectable("Delete")) {
                                                kernel->fs->fdelete(now_tree_node->cat());
                                            }
                                            ImGui::EndPopup();
                                        }
                                        stk.push(now_tree_node);
                                        break;
                                    }
                                    else {
                                        if (ImGui::BeginPopupContextItem(dummy_labels[dummy_index++]))
                                        {
                                            if (ImGui::Selectable("Delete")) {
                                                kernel->fs->fdelete(now_tree_node->cat());
                                            }
                                            ImGui::EndPopup();
                                        }
                                    }
                                    
                                }
                                stk.push(now_tree_node);
                                now_tree_node = now_tree_node->firstchild;
                            }
                            if (stk.size())
                            {
                                now_tree_node = stk.top();
                                stk.pop();
                                if (!now_tree_node->next_sibling) ImGui::TreePop();
                                now_tree_node = now_tree_node->next_sibling;
                            }
                        }
                    }
                    if (file_editor) {
                        if (!mem_set) {
                            memset(buf_fstree, 0, FS::BLK_SIZE* FS::MAX_N_BLKS);
                            kernel->fs->read(fs_node_full_path, buf_fstree, 0, -1);
                            mem_set = true;
                        }
                        ImGui::Separator();
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (float)20);
                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                        ImGui::BeginChild("File Viewer",
                            ImVec2(ImGui::GetWindowContentRegionWidth(), -1), true);
                        ImGui::Text("%s", fs_node_full_path);
                        ImGui::Separator();

                        if (ImGui::Button("Edit File")) {
                            edit = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Save")) {
                            if (edit) {
                                kernel->fs->write(fs_node_full_path,
                                    buf_fstree, 0, strlen(buf_fstree));
                            }
                        }

                        ImGui::Separator();
                        ImGui::InputTextMultiline(fs_node_full_path.c_str(),
                            buf_fstree, FS::BLK_SIZE * FS::MAX_N_BLKS,
                            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                            edit ? ImGuiInputTextFlags_AllowTabInput :
                            ImGuiInputTextFlags_ReadOnly);
                        ImGui::Separator();

                        if (ImGui::Button("Close(Without Saving)")) {
                            file_editor = false;
                            mem_set = false;
                            edit = false;
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleVar();
                    }
                    ImGui::End();

                    ImGui::Begin("Process States", 0, status_flags);
                    if (ImGui::BeginTable("Process States", 11, prtbl_flags)) {
                        ImGui::TableSetupColumn("..");
                        ImGui::TableSetupColumn("Pid");
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("State");
                        ImGui::TableSetupColumn("Parent pid");
                        ImGui::TableSetupColumn("Priority");
                        ImGui::TableSetupColumn("CPU Time");
                        ImGui::TableSetupColumn("Serve Time");
                        ImGui::TableSetupColumn("IO Time");
                        ImGui::TableSetupColumn("ETA");
                        ImGui::TableSetupColumn("PF Rate(%)");
                        ImGui::TableHeadersRow();
                        for (auto v : process_states) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            if (v[0] != "0" && v[0] != "1")  {
                                ImGui::PushID(stoi(v[0]));
                                if (ImGui::SmallButton("..")) {
                                    ImGui::OpenPopup("PrOps");
                                }
                                if (ImGui::BeginPopup("PrOps")) {
                                    if (ImGui::Selectable("Kill")) {
                                        kernel->sch->safe_kill(stoi(v[0]));
                                    }
                                    if (ImGui::Selectable("Force Swapout")) {
                                        ImGui::OpenPopup("Info");
                                    }
                                    if (ImGui::BeginPopupModal("Info", 0,
                                        ImGuiWindowFlags_AlwaysAutoResize)) {
                                        ImGui::Text("Swapping-out Not Implemented Yet.\n");
                                        if (ImGui::Button("OK", ImVec2(120, 0))) {
                                            ImGui::CloseCurrentPopup();
                                        }
                                        ImGui::EndPopup();
                                    }
                                    ImGui::EndPopup();
                                }
                                ImGui::PopID();
                            }
                            
                            for (int i = 0; i < 10; i++) {
                                ImGui::TableSetColumnIndex(i+1);
                                ImGui::Text("%s", v[i].c_str());
                                ImGui::SameLine();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::End();
                }
                {
                    ImGui::Begin("Virtual Memory States", 0, status_flags);
                    if (ImGui::BeginTable("VM States", 8, prtbl_flags)) {
                        ImGui::TableSetupColumn("Pid");
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Refed");
                        ImGui::TableSetupColumn("Present");
                        ImGui::TableSetupColumn("Time mapped");
                        ImGui::TableSetupColumn("Last refed");
                        ImGui::TableSetupColumn("VPage");
                        ImGui::TableSetupColumn("RPage");
                        ImGui::TableHeadersRow();
                        for (auto v : mem_states_per_proc) {
                            ImGui::TableNextRow();
                            if (v.size() == 8) {
                                for (int i = 0; i < 8; i++) {
                                    ImGui::TableSetColumnIndex(i);
                                    ImGui::Text(v[i].c_str());
                                    ImGui::SameLine();
                                }
                            }
                            else {
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text(v[0].c_str());
                                ImGui::SameLine();
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text(v[1].c_str());
                                ImGui::SameLine();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::End();
                }
                {
                    ImGui::Begin("Memory Page Status", 0, status_flags);
                    static ImPlotColormap map = ImPlotColormap_Jet;
                    ImPlot::PushColormap(map);
                    ImPlot::SetNextPlotTicksX(0, 1, map_len, xlabels);
                    ImPlot::SetNextPlotTicksY(1, 0, map_len, xlabels);
                    if (ImPlot::BeginPlot("Memory Mapping", NULL, NULL,
                        ImVec2(200, 200), ImPlotFlags_NoLegend | ImPlotFlags_NoMousePos,
                        axes_flags, axes_flags)) {
                        ImPlot::PlotHeatmap("heat", mmap, map_len, map_len, 0, 0, "%d");
                        ImPlot::EndPlot();
                    }
                    ImGui::SameLine(); 
                    if (ImGui::BeginTable("Swap", 2,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings)) {
                        for (auto i : swap_blks) {
                            ImGui::TableNextColumn();
                            if (i == -1) {
                                ImGui::Button("Empty", ImVec2(-FLT_MIN, -0.0f));
                            }
                            else {
                                ImGui::Button(to_string(i).c_str(), ImVec2(-FLT_MIN, -0.0f));
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::End();
                }
                {
                    ImGui::Begin("Gannt Chart", 0, status_flags);
                    ImPlot::SetNextPlotLimitsX(x_clock - 0.5, 
                        kernel_clock < 5 ? 5 : kernel_clock + 1.5, 
                        ImGuiCond_Once);
                    ImPlot::SetNextPlotLimitsY(-0.5, 
                        max(4.5 , g_history.size() + 0.5), ImGuiCond_Always);
                    ImPlot::SetNextPlotTicksX(x_clock, kernel_clock, 
                        kernel_clock - x_clock + 1, tlabels);
                    ImPlot::SetNextPlotTicksY(0, max(1, g_history.size() - 1),
                        g_history.size(), plabels);
                    ImPlot::FitNextPlotAxes();
                    if (ImPlot::BeginPlot("Gannt Chart")) {
                        ImPlot::SetLegendLocation(ImPlotLocation_North | ImPlotLocation_West,
                            ImPlotOrientation_Vertical, false);
                        ImPlot::SetNextLineStyle(g_color_map[1]);
                        ImPlot::PlotDummy("Ready");
                        ImPlot::SetNextLineStyle(g_color_map[2]);
                        ImPlot::PlotDummy("Running");
                        ImPlot::SetNextLineStyle(g_color_map[3]);
                        ImPlot::PlotDummy("Waiting");
                        int pov = 0;
                        ImDrawList* draw_list = ImPlot::GetPlotDrawList();
                        for (auto &[k, v] : g_history) {
                            for (auto e : v) {
                                {
                                    ImVec2 open_pos = ImPlot::PlotToPixels(e.clock - 0.5,
                                        pov + 0.5);
                                    ImVec2 close_pos = ImPlot::PlotToPixels(e.clock + 0.5,
                                        pov - 0.5);
                                    ImVec2 pltpos = ImPlot::GetPlotPos();
                                    ImVec2 pltsz = ImPlot::GetPlotSize();
                                    if (close_pos.x < pltpos.x) continue;
                                    if (close_pos.y < pltpos.y) continue;
                                    if (open_pos.x > pltpos.x + pltsz.x) continue;
                                    if (open_pos.x < pltpos.x) {
                                        open_pos.x = pltpos.x;
                                    }
                                    if (open_pos.y < pltpos.y) {
                                        open_pos.y = pltpos.y;
                                    }
                                    if (close_pos.x > pltpos.x + pltsz.x) {
                                        close_pos.x = pltpos.x + pltsz.x;
                                    }
                                    
                                    ImU32 color = ImGui::GetColorU32(g_color_map[e.pstate]);
                                    draw_list->AddRectFilled(open_pos, close_pos, color);
                                }
                            }
                            pov++;
                        }
                        if (ImPlot::IsPlotHovered()) {
                            ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                            mouse.x = floor(mouse.x + 0.5);
                            mouse.y = floor(mouse.y + 0.5);
                            float  tool_l = ImPlot::PlotToPixels(mouse.x - 0.5, mouse.y).x;
                            float  tool_r = ImPlot::PlotToPixels(mouse.x + 0.5, mouse.y).x;
                            float  tool_t = ImPlot::PlotToPixels(mouse.x, mouse.y + 0.5).y;
                            float  tool_b = ImPlot::PlotToPixels(mouse.x, mouse.y - 0.5).y;
                            ImPlot::PushPlotClipRect();
                            draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b), IM_COL32(128, 128, 128, 64));
                            ImPlot::PopPlotClipRect();
                            int tick = static_cast<int>(mouse.x) - 1;
                            int pentry = static_cast<int>(mouse.y);
                         
                            if (pentry < g_history.size()) {
                                string name = string(plabels[pentry]);
                                int pid = -1;
                                for (auto& [k, v] : g_history) {
                                    if (v[0].pname == name) {
                                        pid = k;
                                        break;
                                    }
                                }
                                tick = tick - g_history[pid][0].clock + 1;
                                if (pid != -1 
                                    && tick >= 0
                                    && tick < g_history[pid].size()) {
                                    ImGui::BeginTooltip();
                                    ImGui::Text("pid:   %d", pid);
                                    ImGui::Text("name:  %s", name);
                                    ImGui::Text("state: %s", states[g_history[pid][tick].pstate]);
                                    ImGui::Text("clock: %d", g_history[pid][tick].clock);
                                    ImGui::EndTooltip();
                                }
                            }
                        }
                        ImPlot::EndPlot();
                    }
                    ImGui::End();
                }
            }
                
        }
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
