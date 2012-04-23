// This file is part of fityk program. Copyright (C) Marcin Wojdyr
// Licence: GNU General Public License ver. 2+

#include <wx/wx.h>
#include <wx/cmdline.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>
#include <wx/filesys.h>
#include <wx/tooltip.h>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <boost/scoped_ptr.hpp>

#ifndef _WIN32
# include <signal.h>
#endif

#include "app.h"
#include "cmn.h"
#include "frame.h"
#include "plotpane.h"
#include "dataedit.h" //DataEditorDlg::read_transforms()
#include "sidebar.h" // initializations
#include "statbar.h" // initializations
#include "../logic.h"

using namespace std;

IMPLEMENT_APP(FApp)


/// command line options
static const wxCmdLineEntryDesc cmdLineDesc[] = {
#if wxCHECK_VERSION(2, 9, 0)
    { wxCMD_LINE_SWITCH, "h", "help", "show this help message",
                                wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
    { wxCMD_LINE_SWITCH, "V", "version",
          "output version information and exit", wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_OPTION, "c", "cmd", "script passed in as string",
                                                   wxCMD_LINE_VAL_STRING, 0 },
    { wxCMD_LINE_OPTION, "g", "config",
               "choose GUI configuration", wxCMD_LINE_VAL_STRING, 0 },
    { wxCMD_LINE_SWITCH, "I", "no-init",
          "don't process $HOME/.fityk/init file", wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_SWITCH, "r", "reorder",
          "reorder data (50.xy before 100.xy)", wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_PARAM,  0, 0, "script or data file", wxCMD_LINE_VAL_STRING,
                        wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_PARAM_MULTIPLE },
#else
    { wxCMD_LINE_SWITCH, wxT("h"), wxT("help"), wxT("show this help message"),
                                wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
    { wxCMD_LINE_SWITCH, wxT("V"), wxT("version"),
          wxT("output version information and exit"), wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_OPTION, wxT("c"),wxT("cmd"), wxT("script passed in as string"),
                                                   wxCMD_LINE_VAL_STRING, 0 },
    { wxCMD_LINE_OPTION, wxT("g"),wxT("config"),
               wxT("choose GUI configuration"), wxCMD_LINE_VAL_STRING, 0 },
    { wxCMD_LINE_SWITCH, wxT("I"), wxT("no-init"),
          wxT("don't process $HOME/.fityk/init file"), wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_SWITCH, wxT("r"), wxT("reorder"),
          wxT("reorder data (50.xy before 100.xy)"), wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_PARAM,  0, 0, wxT("script or data file"),wxCMD_LINE_VAL_STRING,
                        wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_PARAM_MULTIPLE },
#endif
    { wxCMD_LINE_NONE, 0, 0, 0,  wxCMD_LINE_VAL_NONE, 0 }
};

//---------------- C A L L B A C K S --------------------------------------

void gui_show_message(UserInterface::Style style, const string& s)
{
    frame->output_text(style, s + "\n");
}

string gui_user_input(const string& prompt)
{
    if (contains_element(prompt, "[y/n]")) {
        int r = wxMessageBox(s2wx(prompt), "Query", wxYES_NO);
        if (r == wxYES)
            return "y";
        else if (r == wxNO)
            return "n";
        else
            return "";
    }
    else {
        wxString s = wxGetTextFromUser(s2wx(prompt), "Query");
        return strip_string(wx2s(s));
    }
}

void gui_draw_plot(UserInterface::RepaintMode mode)
{
    bool now = (mode == UserInterface::kRepaintImmediately);
    frame->plot_pane()->refresh_plots(now, kAllPlots);
}

// Switch between normal (=1) and disabled w/ "busy" cursor (0) GUI mode.
// or force wxYield (-1).
// Typically. To be used during time-consuming computations.
// Also used to invoke wxYield();
void gui_hint(int mode)
{
    static wxWindowDisabler *wd = NULL;
    if (mode == 0)
        wd = new wxWindowDisabler();
    else if (mode == 1) {
        delete wd;
        wd = NULL;
    }
    else if (mode == -1)
        wxYield();
}


UserInterface::Status gui_exec_command(const string& s)
{
    //FIXME should I limit number of displayed lines?
    //const int max_lines_in_output_win = 1000;
    //don't output plot command - it is generated by every zoom in/out etc.
    bool output = strncmp(s.c_str(), "plot", 4) != 0;
    if (output)
        frame->output_text(UserInterface::kInput, "=-> " + s + "\n");
    else
        frame->set_status_text(s);
    wxBusyCursor wait;
    UserInterface::Status r;
    try {
        r = ftk->ui()->execute_line(s);
    }
    catch(ExitRequestedException) {
        frame->Close(true);
        return UserInterface::kStatusOk;
    }
    frame->after_cmd_updates();
    return r;
}
//-------------------------------------------------------------------------

void interrupt_handler (int /*signum*/)
{
    //set flag for breaking long computations
    fityk::user_interrupt = true;
}

void write_white_config(wxConfigBase *w)
{
    cfg_write_color(w, "MainPlot/Colors/bg", wxColour(255, 255, 255));
    cfg_write_color(w, "MainPlot/Colors/model", wxColour(0, 0, 127));
    cfg_write_color(w, "MainPlot/Colors/xAxis", wxColour(0, 0, 0));
    cfg_write_color(w, "MainPlot/Colors/data/0", wxColour(0, 127, 0));
    cfg_write_color(w, "MainPlot/Colors/peak/0", wxColour(255, 89, 89));
    cfg_write_color(w, "AuxPlot_0/Colors/bg", wxColour(255, 255, 255));
    cfg_write_color(w, "AuxPlot_0/Colors/active_data", wxColour(0, 127, 0));
    cfg_write_color(w, "AuxPlot_0/Colors/xAxis", wxColour(0, 0, 0));
    cfg_write_color(w, "AuxPlot_1/Colors/bg", wxColour(255, 255, 255));
    cfg_write_color(w, "AuxPlot_1/Colors/active_data", wxColour(0, 127, 0));
    cfg_write_color(w, "AuxPlot_1/Colors/xAxis", wxColour(0, 0, 0));
    cfg_write_color(w, "OutputWin/Colors/normal", wxColour(51, 51, 51));
    cfg_write_color(w, "OutputWin/Colors/warn", wxColour(172, 0, 0));
    cfg_write_color(w, "OutputWin/Colors/quot", wxColour(46, 58, 107));
    cfg_write_color(w, "OutputWin/Colors/input", wxColour(0, 76, 9));
    cfg_write_color(w, "OutputWin/Colors/bg", wxColour(255, 255, 255));
}


bool FApp::OnInit(void)
{
#ifndef _WIN32
    // setting Ctrl-C handler
    if (signal (SIGINT, interrupt_handler) == SIG_IGN)
        signal (SIGINT, SIG_IGN);
#endif //_WIN32

    SetAppName(wxT("fityk"));

    // if options can be parsed
    wxCmdLineParser cmdLineParser(cmdLineDesc, argc, argv);
    if (cmdLineParser.Parse(false) != 0) {
        cmdLineParser.Usage();
        return false; //false = exit the application
    }
    else if (cmdLineParser.Found(wxT("V"))) {
        wxMessageOutput::Get()->Printf(wxT("fityk version %s\n"), wxT(VERSION));
        return false; //false = exit the application
    } //the rest of options will be processed in process_argv()

    ftk = new Ftk;

    // set callbacks
    ftk->ui()->connect_show_message(gui_show_message);
    ftk->ui()->connect_draw_plot(gui_draw_plot);
    ftk->ui()->connect_hint_ui(gui_hint);
    ftk->ui()->connect_exec_command(gui_exec_command);
    ftk->ui()->connect_user_input(gui_user_input);

    wxImage::AddHandler(new wxPNGHandler);

    //global settings
#if wxUSE_TOOLTIPS
    wxToolTip::Enable (true);
    wxToolTip::SetDelay (500);
#endif

    //create user data directory, if it doesn't exists
    wxString fityk_dir = wxStandardPaths::Get().GetUserDataDir();
    if (!wxDirExists(fityk_dir))
        wxMkdir(fityk_dir);

    wxConfig::DontCreateOnDemand();

    // set config file for options automatically saved
    // it will be accessed only via wxConfig::Get()
    wxFileConfig *config = new wxFileConfig(wxEmptyString, wxEmptyString,
                                            get_conf_file("wxoptions"));
    wxConfig::Set(config);

    // directory for configs
    config_dir = fityk_dir + wxFILE_SEP_PATH + wxT("configs") + wxFILE_SEP_PATH;
    if (!wxDirExists(config_dir)) {
        wxMkdir(config_dir);
        // create white-background config
        boost::scoped_ptr<wxFileConfig> w(new wxFileConfig("", "",
                                                           config_dir+"white"));
        write_white_config(w.get());
    }

    // moving configs from ver. <= 0.9.7 to the current locations
    wxString old_config = get_conf_file("config");
    if (wxFileExists(old_config))
        wxRenameFile(old_config, config_dir + wxT("default"), false);
    wxString old_alt_config = get_conf_file("alt-config");
    if (wxFileExists(old_alt_config))
        wxRenameFile(old_alt_config, config_dir + wxT("alt-config"), false);

    EditTransDlg::read_transforms(false);

    // Create the main frame window
    frame = new FFrame(NULL, -1, wxT("fityk"), wxDEFAULT_FRAME_STYLE);

    wxString ini_conf = wxT("default");
    // if the -g option was given, it replaces default config
    cmdLineParser.Found(wxT("g"), &ini_conf);
    wxConfigBase *cf = new wxFileConfig(wxT(""), wxT(""), config_dir+ini_conf);
    frame->read_all_settings(cf);

    frame->Show(true);

    // sash inside wxNoteBook can have wrong position (eg. wxGTK 2.7.1)
    frame->sidebar_->read_settings(cf);
    // sash on the status bar is also in the wrong place (wxGTK),
    // because for some reason wxSplitterWindow had width=0 before Show()
    frame->status_bar_->read_settings(cf);

    delete cf;

    SetTopWindow(frame);

    if (!cmdLineParser.Found(wxT("I"))) {
        // run initial commands
        wxString startup_file = get_conf_file(fityk::startup_commands_filename);
        if (wxFileExists(startup_file)) {
            ftk->ui()->exec_script(wx2s(startup_file));
        }
    }

    process_argv(cmdLineParser);

    frame->after_cmd_updates();
    return true;
}


int FApp::OnExit()
{
    delete ftk;
    wxConfig::Get()->Write(wxT("/FitykVersion"), pchar2wx(VERSION));
    delete wxConfig::Set((wxConfig *) NULL);
    return 0;
}

#ifdef __WXMAC__
#include <wx/msgdlg.h>
void FApp::MacOpenFile(const wxString &filename)
{
    try {
        ftk->ui()->process_cmd_line_filename(wx2s(filename));
    }
    catch (runtime_error const& e) {
        wxMessageBox(s2wx(e.what()), "Open File Error", wxOK|wxICON_ERROR);
    }
}
#endif

namespace {

struct less_filename : public binary_function<string, string, bool> {
    int n;
    less_filename(int n_) : n(n_) {}
    bool operator()(string x, string y)
    {
        if (isdigit(x[n]) && isdigit(y[n])) {
            string xc(x, n), yc(y, n);
            return strtod(xc.c_str(), 0) < strtod(yc.c_str(), 0);
        }
        else
            return x < y;
    }
};

int find_common_prefix_length(vector<string> const& p)
{
    assert(p.size() > 1);
    for (size_t n = 0; n < p.begin()->size(); ++n)
        for (vector<string>::const_iterator i = p.begin()+1; i != p.end(); ++i)
            if (n >= i->size() || (*i)[n] != (*p.begin())[n])
                return n;
    return p.begin()->size();
}

} // anonymous namespace

/// parse and execute command line switches and arguments
void FApp::process_argv(wxCmdLineParser &cmdLineParser)
{
    wxString cmd;
    if (cmdLineParser.Found(wxT("c"), &cmd))
        ftk->ui()->exec_and_log(wx2s(cmd));
    //the rest of parameters/arguments are scripts and/or data files
    vector<string> p;
    for (unsigned int i = 0; i < cmdLineParser.GetParamCount(); i++)
        p.push_back(wx2s(cmdLineParser.GetParam(i)));
    if (cmdLineParser.Found(wxT("r")) && p.size() > 1) { // reorder
        sort(p.begin(), p.end(), less_filename(find_common_prefix_length(p)));
    }
    for (vector<string>::const_iterator i = p.begin(); i != p.end(); ++i) {
        try {
            ftk->ui()->process_cmd_line_arg(*i);
        }
        catch (runtime_error const& e) {
            fprintf(stderr, "Error: %s\n", e.what());
            exit(1);
        }
    }
    if (ftk->get_dm_count() > 1) {
        frame->SwitchSideBar(true);
        // zoom to show all loaded datafiles
        RealRange r;
        ftk->view.change_view(r, r, range_vector(0, ftk->get_dm_count()));
    }
}

// search for `name' in two or three directories:
//   wxStandardPaths::GetResourcesDir()
//                        on Mac: appname.app/Contents/Resources bundle subdir
//                        on Win: dir where executable is
//   HELP_DIR = $(pkgdatadir), not defined on Win
//   {exedir}/../../doc/ and {exedir}/../../../doc/ - for uninstalled program
wxString get_help_url(const wxString& name)
{
    wxString dir = wxFILE_SEP_PATH + wxString(wxT("html"));
    wxPathList paths;
    // installed path
#if defined(__WXMAC__) || defined(__WXMSW__)
    paths.Add(wxStandardPaths::Get().GetResourcesDir() + dir);
#endif
#ifdef HELP_DIR
    paths.Add(wxT(HELP_DIR) + dir);
#endif
    // uninstalled paths, relative to executable
    wxString up = wxFILE_SEP_PATH + wxString(wxT(".."));
    paths.Add(wxPathOnly(wxGetApp().argv[0]) + up + up
              + wxFILE_SEP_PATH + wxT("doc") + dir);
    paths.Add(wxPathOnly(wxGetApp().argv[0]) + up + up + up
              + wxFILE_SEP_PATH + wxT("doc") + dir);

    wxString path = paths.FindAbsoluteValidPath(name);
    if (!path.IsEmpty())
        return wxFileSystem::FileNameToURL(path);
    else
        return wxT("http://fityk.nieto.pl/") + name;
}

wxString get_sample_path(const wxString& name)
{
    wxString dir = wxFILE_SEP_PATH + wxString(wxT("samples"));
    wxPathList paths;
    // installed path
#if defined(__WXMAC__) || defined(__WXMSW__)
    paths.Add(wxStandardPaths::Get().GetResourcesDir() + dir);
#endif
#ifdef HELP_DIR
    paths.Add(wxT(HELP_DIR) + dir);
#endif
    // uninstalled paths, relative to executable
    wxString up = wxFILE_SEP_PATH + wxString(wxT(".."));
    paths.Add(wxPathOnly(wxGetApp().argv[0]) + up + up + dir);
    paths.Add(wxPathOnly(wxGetApp().argv[0]) + up + up + up + dir);
    wxFileName path(paths.FindAbsoluteValidPath(name));
    path.Normalize(wxPATH_NORM_DOTS);
    return path.GetFullPath();
}

#ifdef __WXMAC__
void open_new_instance()
{
    string res = wx2s(wxStandardPaths::Get().GetResourcesDir());
    // it has "/Contents/Resources" (19 chars) after bundle.app
    if (res.size() > 19)
        system(("open -n " + res.substr(0, res.size()-19)).c_str());
}
#endif

