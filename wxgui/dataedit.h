// This file is part of fityk program. Copyright (C) Marcin Wojdyr
// Licence: GNU General Public License ver. 2+
/// EditTransDlg: Data > Edit Transformations

#ifndef FITYK_WX_DATAEDIT_H_
#define FITYK_WX_DATAEDIT_H_

#include <string>
#include <vector>

namespace fityk { class Data; }

struct DataTransform
{
    wxString name;
    wxString description;
    wxString code;
    bool in_menu;
    bool is_changed;

    DataTransform(const wxString& name_, const wxString& description_,
                  const wxString& code_, bool in_menu_=false)
        : name(name_), description(description_),
          code(code_), in_menu(in_menu_), is_changed(false) {}
    DataTransform(const std::string& line);
    std::string as_fileline();
    wxString get_display_name() const
        { return is_changed ? wxT("*") + name : name; }
};


class EditTransDlg : public wxDialog
{
    friend class DataTable;
    typedef std::vector<std::pair<int,fityk::Data*> > ndnd_type;
public:
    EditTransDlg (wxWindow* parent, wxWindowID id, ndnd_type const& dd);
    static std::vector<DataTransform> const& get_transforms()
                                                    { return transforms; }
    static void read_transforms(bool skip_file);
    static void execute_tranform(std::string const& code);

private:
    static std::vector<DataTransform> transforms;
    ndnd_type ndnd;
    wxCheckListBox *trans_list;
    wxTextCtrl *name_tc, *description_tc, *code_tc;
    wxButton *add_btn, *remove_btn, *up_btn, *down_btn,
             *save_btn, *revert_btn, *todefault_btn,
             *apply_btn, *rezoom_btn, *undo_btn, *help_btn;

    void OnAdd(wxCommandEvent& event);
    void OnRemove(wxCommandEvent& event);
    void OnUp(wxCommandEvent& event);
    void OnDown(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnRevert(wxCommandEvent& event);
    void OnToDefault(wxCommandEvent& event);
    void OnApply(wxCommandEvent& event);
    void OnReZoom(wxCommandEvent& event);
    void OnUndo(wxCommandEvent&);
    void OnNameText(wxCommandEvent&);
    void OnDescText(wxCommandEvent&);
    void OnCodeText(wxCommandEvent&);
    void OnListItemSelected(wxCommandEvent&) { update_right_side(); }
    void OnListItemToggled(wxCommandEvent& event);

    void init();
    void initialize_checklist();
    bool update_apply_button();
    void update_right_side();
    static std::string conv_code_to_one_line(std::string const& code);
};

#endif // FITYK_WX_DATAEDIT_H_

