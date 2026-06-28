#pragma once
#include <string>
#include <map>

// ============================================================
// I18n - Internationalization (Chinese/English)
// ============================================================

enum class Language { ZH, EN };

class I18n {
public:
    static I18n& instance() {
        static I18n inst;
        return inst;
    }

    void setLanguage(Language lang) { current = lang; }
    Language getLanguage() const { return current; }
    void toggleLanguage() { current = (current == Language::ZH) ? Language::EN : Language::ZH; }

    std::string t(const std::string& key) const {
        auto& table = (current == Language::ZH) ? zh : en;
        auto it = table.find(key);
        if (it != table.end()) return it->second;
        return key;
    }

private:
    I18n() : current(Language::ZH) {}

    Language current;
    std::map<std::string, std::string> zh = {
        {"create", "创建"}, {"manage", "管理"},
        {"sphere", "球体"}, {"cube", "立方体"}, {"cylinder", "圆柱体"},
        {"cone", "圆锥体"}, {"torus", "圆环体"},
        {"lineSegment", "线段"}, {"circularArc", "圆弧"},
        {"rectangle", "矩形"}, {"circle", "圆形"},
        {"triangle", "三角形"}, {"polygon", "多边形"},
        {"deleteSelected", "删除选中"}, {"clearAll", "清空全部"},
        {"shapeList", "图形列表"}, {"noShapes", "暂无图形"},
        {"type", "类型"}, {"actions", "操作"},
        {"hide", "隐藏"}, {"show", "显示"}, {"delete", "删除"},
        {"totalShapes", "图形数量"}, {"selectedShape", "已选择"},
        {"unknown", "未知"}, {"language", "语言"},
        {"chinese", "中文"}, {"english", "English"},
        {"appName", "GeomTool"}, {"version", "v1.0"},
        {"id", "ID"},
    };
    std::map<std::string, std::string> en = {
        {"create", "Create"}, {"manage", "Manage"},
        {"sphere", "Sphere"}, {"cube", "Cube"}, {"cylinder", "Cylinder"},
        {"cone", "Cone"}, {"torus", "Torus"},
        {"lineSegment", "Line Segment"}, {"circularArc", "Circular Arc"},
        {"rectangle", "Rectangle"}, {"circle", "Circle"},
        {"triangle", "Triangle"}, {"polygon", "Polygon"},
        {"deleteSelected", "Delete Selected"}, {"clearAll", "Clear All"},
        {"shapeList", "Shape List"}, {"noShapes", "No shapes"},
        {"type", "Type"}, {"actions", "Actions"},
        {"hide", "Hide"}, {"show", "Show"}, {"delete", "Delete"},
        {"totalShapes", "Total Shapes"}, {"selectedShape", "Selected"},
        {"unknown", "Unknown"}, {"language", "Language"},
        {"chinese", "中文"}, {"english", "English"},
        {"appName", "GeomTool"}, {"version", "v1.0"},
        {"id", "ID"},
    };
};
