#pragma once
#include "kernel.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

struct TreeItem
{
    string name;
    int size;
    int type;
    TreeItem* parent;
    TreeItem* firstchild;
    TreeItem* next_sibling;

    TreeItem() :
        firstchild(nullptr),
        next_sibling(nullptr),
        parent(nullptr),
        name(""),
        size(0),
        type(-1)
    {}

    ~TreeItem() {
        delete firstchild;
        delete next_sibling;
    }

    TreeItem(const string& name, const int& size, const int& type, TreeItem* parent) :
        firstchild(nullptr),
        next_sibling(nullptr),
        parent(parent),
        name(name),
        size(size),
        type(type)
    {}

    TreeItem(TreeItem* cp) :
        firstchild(cp->firstchild),
        next_sibling(cp->next_sibling),
        parent(cp->parent),
        name(cp->name),
        size(cp->size),
        type(cp->type)
    {}

    TreeItem* lastChild()
    {
        TreeItem* nextChild = firstchild, * child = nullptr;

        while (nextChild != nullptr)
        {
            child = nextChild;
            nextChild = nextChild->next_sibling;
        }

        return child;
    }

    TreeItem* find(const string& entry)
    {
        TreeItem* tmp = firstchild;

        while (tmp != nullptr && tmp->name != entry)
            tmp = tmp->next_sibling;

        return tmp;
    }

    void add(const string& value, const int& size, const int& type)
    {
        if (firstchild == nullptr)
            firstchild = new TreeItem(value, size, type, this);
        else
            lastChild()->next_sibling = new TreeItem(value, size, type, this);
    }

    string cat() {
        TreeItem* p = this->parent;
        stack<string> chain;
        while (p) {
            chain.push(p->name);
            p = p->parent;
        }
        string res{};
        string tmp;
        while (chain.size()) {
            tmp = chain.top();
            chain.pop();
            if (tmp[tmp.size() - 1] == '/') 
                tmp = tmp.substr(0, tmp.size() - 1);
            res += tmp;
            res += "/";
        }
        res += this->name;
        return res;
    }
};

static ImGuiTableFlags prtbl_flags = ImGuiTableFlags_Resizable
| ImGuiTableFlags_Reorderable
| ImGuiTableFlags_Hideable
| ImGuiTableFlags_BordersOuter
| ImGuiTableFlags_BordersV
| ImGuiTableFlags_RowBg
| ImGuiTableFlags_SizingFixedFit
| ImGuiTableFlags_SortMulti;

static ImGuiWindowFlags clk_flags = ImGuiWindowFlags_NoScrollbar
| ImGuiWindowFlags_NoCollapse
//| ImGuiWindowFlags_NoMove
| ImGuiWindowFlags_NoResize;
static ImGuiWindowFlags status_flags = ImGuiWindowFlags_NoCollapse;
static ImPlotAxisFlags axes_flags = ImPlotAxisFlags_Lock
| ImPlotAxisFlags_NoGridLines
| ImPlotAxisFlags_NoTickMarks;
static ImGuiTabBarFlags tabs_flags = ImGuiTabBarFlags_Reorderable
| ImGuiTabBarFlags_FittingPolicyResizeDown;

void main_window(Kernel* kernel);