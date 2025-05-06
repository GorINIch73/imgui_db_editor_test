#include "database.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
// #include <nfd.h>
// #include <regex>
#include <string>
#include <vector>

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// настройки скалирования интерфейса
float get_dpi_scale(GLFWwindow *window) {
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    return (xscale + yscale) / 2.0f; // Средний масштаб
}

// реализация диалога выбора файла
namespace fs = std::filesystem;

const char *filterPresets[] = {"Все файлы (*.*)", "Текстовые файлы (*.txt)",
                               "Изображения (*.png;*.jpg;*.jpeg;*.bmp)",
                               "Документы (*.pdf;*.doc;*.docx)",
                               "Базы данных (*.db)"};

const char *filterPatterns[] = {"*", "*.txt", "*.png;*.jpg;*.jpeg;*.bmp",
                                "*.pdf;*.doc;*.docx", "*.db"};

class FileBrowser {
    public:
        std::string currentPath = fs::current_path().string();
        std::string selectedFile;
        std::vector<std::string> directories;
        std::vector<std::string> files;
        std::string filter = "*.db";    // Маска по умолчанию - все файлы
        char filterInput[128] = "*.db"; // Буфер для ввода маски

        void Refresh() {
            directories.clear();
            files.clear();

            for (const auto &entry : fs::directory_iterator(currentPath)) {
                try {
                    if (entry.is_directory()) {
                        directories.push_back(entry.path().filename().string());
                    } else if (MatchesFilter(
                                   entry.path().filename().string())) {
                        files.push_back(entry.path().filename().string());
                    }
                } catch (...) {
                    // Пропустить файлы с проблемами доступа
                }
            }
        }
        bool MatchesFilter(const std::string &filename) {
            std::vector<std::string> masks;
            std::istringstream iss(filter);
            std::string mask;

            while (std::getline(iss, mask, ';')) {
                if (mask == "*")
                    return true;

                if (mask.find("*.") == 0) {
                    std::string ext = mask.substr(1);
                    if (filename.size() >= ext.size() &&
                        filename.compare(filename.size() - ext.size(),
                                         ext.size(), ext) == 0) {
                        return true;
                    }
                } else if (filename.find(mask) != std::string::npos) {
                    return true;
                }
            }

            return false;
        }

        bool Draw() {
            bool fileSelected = false;

            // Панель управления с фильтром
            if (ImGui::Button("Наверх") && currentPath != "/") {
                currentPath = fs::path(currentPath).parent_path().string();
                Refresh();
            }

            ImGui::SameLine();
            ImGui::Text("Текущая папка: %s", currentPath.c_str());

            if (ImGui::BeginCombo("Предустановки", filter.c_str())) {
                for (int i = 0; i < IM_ARRAYSIZE(filterPresets); i++) {
                    if (ImGui::Selectable(filterPresets[i],
                                          filter == filterPatterns[i])) {
                        filter = filterPatterns[i];
                        strcpy(filterInput, filter.c_str());
                        Refresh();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::BeginChild("##browser", ImVec2(0, 300), true);

            // Показываем папки
            for (const auto &dir : directories) {
                if (ImGui::Selectable(("[D] " + dir).c_str(), false,
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        currentPath = (fs::path(currentPath) / dir).string();
                        Refresh();
                    }
                }
            }

            // Показываем файлы
            for (const auto &file : files) {
                if (ImGui::Selectable(file.c_str(), selectedFile == file)) {
                    selectedFile = file;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        fileSelected = true;
                    }
                }
            }

            ImGui::EndChild();

            return fileSelected;
        }
};

// Глобальные переменные для управления окном
bool showFileBrowser = false;
FileBrowser browser; // Наш класс файлового браузера

// Наша база данных
Database db;
std::string dbPath;
bool dbOpen = false;

// Состояние интерфейса
std::vector<std::string> tables;
std::string currentTable;
std::vector<std::map<std::string, std::string>> records;
std::vector<ColumnInfo> tableInfo;
int selectedRecord = -1;
std::map<std::string, std::string> editValues;
bool inTransaction = false;

// выбор файла
void RenderFileBrowser() {
    if (showFileBrowser) {
        // Настройки окна файлового браузера
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Выбор файла", &showFileBrowser)) {
            if (browser.Draw()) // Если файл выбран
            {
                std::string selectedFile =
                    (fs::path(browser.currentPath) / browser.selectedFile);
                printf("Выбран файл: %s\n", selectedFile.c_str());
                // Здесь можно обработать выбранный файл

                showFileBrowser = false; // Закрываем окно после выбора
            }

            // Кнопка "Открыть" для подтверждения выбора
            if (ImGui::Button("Открыть") && !browser.selectedFile.empty()) {
                std::string selectedFile =
                    (fs::path(browser.currentPath) / browser.selectedFile);
                printf("Выбран файл: %s\n", selectedFile.c_str());

                std::string newPath =
                    (fs::path(browser.currentPath) / browser.selectedFile);
                // открываем базу
                if (!newPath.empty()) {
                    dbPath = newPath;
                    dbOpen = db.open(dbPath);
                    if (dbOpen) {
                        tables = db.getTables();
                        if (!tables.empty()) {
                            currentTable = tables[0];
                            tableInfo = db.getTableInfo(currentTable);
                            records =
                                db.query("SELECT * FROM " + currentTable + ";");
                        } else {
                            currentTable.clear();
                            tableInfo.clear();
                            records.clear();
                        }
                        selectedRecord = -1;
                        editValues.clear();
                    }
                }

                showFileBrowser = false;
            }

            ImGui::SameLine();

            // Кнопка отмены
            if (ImGui::Button("Отмена")) {
                showFileBrowser = false;
            }
        }
        ImGui::End();
    }
}

int main(int argc, char **argv) {
    // Инициализация GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

    // Установка версии OpenGL
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Создание окна
    GLFWwindow *window =
        glfwCreateWindow(1280, 720, "Database Editor", nullptr, nullptr);
    if (window == nullptr) {
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Включение VSync

    // Инициализация IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    float scale = get_dpi_scale(window);
    ImGui::GetStyle().ScaleAllSizes(scale); // Масштабирование стилей
    io.FontGlobalScale = scale;             // Масштабирование шрифтов
    io.DisplayFramebufferScale = ImVec2(scale, scale);

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // грузим русский шрифт
    ImFont *font = io.Fonts->AddFontFromFileTTF(
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", 24.0f, nullptr,
        io.Fonts->GetGlyphRangesCyrillic());

    // Настройка стиля
    ImGui::StyleColorsDark();

    // Инициализация рендерера
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Начало кадра IMGUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Главное окно
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Database Editor", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_MenuBar);

        // ImGui::Begin("Database Editor", nullptr,);
        // Меню
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Database...")) {
                    // std::string newPath = OpenFileDialog();
                    showFileBrowser = true;
                    browser.Refresh(); // Обновляем список файлов при открытии
                                       // if (file.Draw()) {
                    //                        std::string newPath =
                    //                            (fs::path(file.currentPath) /
                    //                            file.selectedFile)
                    //                                .string();
                }

                if (ImGui::MenuItem("Close Database", nullptr, false, dbOpen)) {
                    db.close();
                    dbOpen = false;
                    tables.clear();
                    currentTable.clear();
                    records.clear();
                    tableInfo.clear();
                    selectedRecord = -1;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit")) {
                    glfwSetWindowShouldClose(window, true);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Transaction", dbOpen)) {
                if (ImGui::MenuItem("Begin Transaction", nullptr, false,
                                    !inTransaction)) {
                    inTransaction = db.beginTransaction();
                }

                if (ImGui::MenuItem("Commit", nullptr, false, inTransaction)) {
                    inTransaction = !db.commitTransaction();
                }

                if (ImGui::MenuItem("Rollback", nullptr, false,
                                    inTransaction)) {
                    inTransaction = !db.rollbackTransaction();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Статус базы данных
        ImGui::Text("Database: %s", dbOpen ? dbPath.c_str() : "Not opened");
        if (inTransaction) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), " (Transaction active)");
        }

        // диалог выбора файла
        //        if (!dbOpen) {
        RenderFileBrowser();
        //        }

        // Выбор таблицы
        if (dbOpen && !tables.empty()) {
            if (ImGui::BeginCombo("Table", currentTable.c_str())) {
                for (const auto &table : tables) {
                    bool isSelected = (currentTable == table);
                    if (ImGui::Selectable(table.c_str(), isSelected)) {
                        currentTable = table;
                        tableInfo = db.getTableInfo(currentTable);
                        records =
                            db.query("SELECT * FROM " + currentTable + ";");
                        selectedRecord = -1;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        // Разделение на две панели
        if (dbOpen && !currentTable.empty()) {
            float panelHeight = ImGui::GetContentRegionAvail().y - 40;

            // Левая панель - таблица с записями
            ImGui::BeginChild(
                "Records",
                ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, panelHeight),
                true);

            if (ImGui::BeginTable(
                    "RecordsTable", tableInfo.size(),
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                // Заголовки столбцов
                for (const auto &col : tableInfo) {
                    ImGui::TableSetupColumn(col.name.c_str());
                }
                ImGui::TableHeadersRow();

                // Строки с данными
                for (int i = 0; i < records.size(); i++) {
                    ImGui::TableNextRow();
                    bool isSelected = (selectedRecord == i);

                    for (int j = 0; j < tableInfo.size(); j++) {
                        ImGui::TableSetColumnIndex(j);
                        const auto &colName = tableInfo[j].name;
                        const auto &record = records[i];

                        if (record.find(colName) != record.end()) {
                            if (j == 0) {
                                if (ImGui::Selectable(
                                        record.at(colName).c_str(), isSelected,
                                        ImGuiSelectableFlags_SpanAllColumns)) {
                                    selectedRecord = i;
                                    editValues = records[i];
                                }
                            } else {
                                ImGui::Text("%s", record.at(colName).c_str());
                            }
                        }
                    }
                }

                ImGui::EndTable();
            }

            // Кнопки для управления записями
            if (ImGui::Button("Add Record")) {
                editValues.clear();
                for (const auto &col : tableInfo) {
                    editValues[col.name] = "";
                }
                selectedRecord = -1;
            }

            ImGui::SameLine();

            if (ImGui::Button("Delete Record")) {
                if (selectedRecord >= 0 && selectedRecord < records.size()) {
                    std::string where;
                    for (const auto &col : tableInfo) {
                        if (col.primary_key) {
                            if (!where.empty())
                                where += " AND ";
                            where += col.name + " = '" +
                                     records[selectedRecord][col.name] + "'";
                        }
                    }

                    if (where.empty()) {
                        // Если нет первичного ключа, используем все поля
                        // для идентификации записи
                        for (const auto &col : tableInfo) {
                            if (!where.empty())
                                where += " AND ";
                            where += col.name + " = '" +
                                     records[selectedRecord][col.name] + "'";
                        }
                    }

                    if (db.deleteRecord(currentTable, where)) {
                        records =
                            db.query("SELECT * FROM " + currentTable + ";");
                        selectedRecord = -1;
                    }
                }
            }

            ImGui::EndChild();

            ImGui::SameLine();

            // Правая панель - редактирование записи
            ImGui::BeginChild("Edit", ImVec2(0, panelHeight), true);

            if (selectedRecord >= 0 || !editValues.empty()) {
                ImGui::Text("Edit Record");
                ImGui::Separator();

                for (const auto &col : tableInfo) {
                    if (editValues.find(col.name) != editValues.end()) {
                        char buffer[256] = {0};
                        strncpy(buffer, editValues[col.name].c_str(),
                                sizeof(buffer));

                        ImGui::Text("%s (%s):", col.name.c_str(),
                                    col.type.c_str());
                        if (ImGui::InputText(("##" + col.name).c_str(), buffer,
                                             sizeof(buffer))) {
                            editValues[col.name] = buffer;
                        }
                    }
                }

                if (ImGui::Button("Save")) {
                    if (selectedRecord >= 0) {
                        // Обновление существующей записи
                        std::string where;
                        for (const auto &col : tableInfo) {
                            if (col.primary_key) {
                                if (!where.empty())
                                    where += " AND ";
                                where += col.name + " = '" +
                                         records[selectedRecord][col.name] +
                                         "'";
                            }
                        }

                        if (where.empty()) {
                            // Если нет первичного ключа, используем все
                            // поля для идентификации записи
                            for (const auto &col : tableInfo) {
                                if (!where.empty())
                                    where += " AND ";
                                where += col.name + " = '" +
                                         records[selectedRecord][col.name] +
                                         "'";
                            }
                        }

                        if (db.updateRecord(currentTable, editValues, where)) {
                            records =
                                db.query("SELECT * FROM " + currentTable + ";");
                        }
                    } else {
                        // Добавление новой записи
                        if (db.addRecord(currentTable, editValues)) {
                            records =
                                db.query("SELECT * FROM " + currentTable + ";");
                            editValues.clear();
                        }
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel")) {
                    editValues.clear();
                    selectedRecord = -1;
                }
            } else {
                ImGui::Text("Select a record to edit or click 'Add Record'");
            }

            ImGui::EndChild();
        } else if (dbOpen && currentTable.empty()) {
            ImGui::Text("No tables in database");
        } else {
            ImGui::Text("Open a database to begin");
        }

        ImGui::End();

        // Рендеринг
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
