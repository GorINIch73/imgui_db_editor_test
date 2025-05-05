#include "database.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

float get_dpi_scale(GLFWwindow *window) {
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    return (xscale + yscale) / 2.0f; // Средний масштаб
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
                    // В реальном приложении здесь должен быть диалог выбора
                    // файла
                    dbPath = "test.db"; // Пример пути
                    dbOpen = db.open(dbPath);
                    if (dbOpen) {
                        tables = db.getTables();
                        if (!tables.empty()) {
                            currentTable = tables[0];
                            tableInfo = db.getTableInfo(currentTable);
                            records =
                                db.query("SELECT * FROM " + currentTable + ";");
                        }
                    }
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
                        // Если нет первичного ключа, используем все поля для
                        // идентификации записи
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
                            // Если нет первичного ключа, используем все поля
                            // для идентификации записи
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
