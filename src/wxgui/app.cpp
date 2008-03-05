// This file is part of fityk program. Copyright (C) Marcin Wojdyr
// Licence: GNU General Public License version 2
// $Id$

#include <wx/wxprec.h>
#ifdef __BORLANDC__
#pragma hdrstop
#endif
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/cmdline.h> 
#include <wx/config.h>
#include <wx/fs_zip.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>
#if wxUSE_TOOLTIPS
    #include <wx/tooltip.h>
#endif

#include <vector> 
#include <string> 

#include "app.h"
#include "cmn.h"
#include "frame.h"
#include "dataedit.h" //DataEditorDlg::read_transforms()
#include "pane.h" // initializations
#include "sidebar.h" // initializations
#include "../logic.h"
#include "../cmd.h"

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

void gui_show_message(OutputStyle style, const string& s)
{
    frame->output_text(style, s + "\n");
}

void gui_do_draw_plot(bool now)
{
    frame->refresh_plots(now);
}

void gui_wait(float seconds) 
{  
    wxMilliSleep(iround(seconds*1e3)); 
}

void gui_refresh()
{
    wxSafeYield();
}

Commands::Status gui_exec_command(const string& s)
{
    //FIXME should I limit number of displayed lines?
    //const int max_lines_in_output_win = 1000;
    //don't output plot command - it is generated by every zoom in/out etc.
    bool output = strncmp(s.c_str(), "plot", 4) != 0;
    if (output)
        frame->output_text(os_input, "=-> " + s + "\n");
    else
        frame->set_status_text(s);
    wxBusyCursor wait;
    Commands::Status r;
    try {
        r = parse_and_execute(s);
    }
    catch(ExitRequestedException) {
        frame->Close(true);
        return Commands::status_ok;
    }
    frame->after_cmd_updates();
    return r;
}
//-------------------------------------------------------------------------


bool FApp::OnInit(void)
{
    SetAppName(wxT("fityk"));

    // if options can be parsed
    wxCmdLineParser cmdLineParser(cmdLineDesc, argc, argv);
    if (cmdLineParser.Parse(false) != 0) {
        cmdLineParser.Usage();
        return false; //false = exit the application
    }
    else if (cmdLineParser.Found(wxT("V"))) {
        wxMessageOutput::Get()->Printf(wxT("fityk version ") 
                                       + pchar2wx(VERSION) + wxT("\n"));
        return false; //false = exit the application
    } //the rest of options will be processed in process_argv()

    ftk = new Ftk; 

    // set callbacks
    ftk->get_ui()->set_show_message(gui_show_message);
    ftk->get_ui()->set_do_draw_plot(gui_do_draw_plot);
    ftk->get_ui()->set_wait(gui_wait);
    ftk->get_ui()->set_refresh(gui_refresh);
    ftk->get_ui()->set_exec_command(gui_exec_command);

    wxFileSystem::AddHandler(new wxZipFSHandler);
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
    //prefix for wxConfig. It can be wxFileConfig or wxRegConfig,
    // so the prefix on MSW can't be absolute
    // On Unix - it is compatible with get_user_conffile()
    // On MSW - not compatible, but it uses registry, so it doesn't matter
#ifdef __WXMAC__
    wxString prefix = wxStandardPaths::Get().GetUserConfigDir() 
                         + wxFILE_SEP_PATH + wxT("pl.waw.unipress.fityk.");
#else
    wxString prefix = pchar2wx(config_dirname) + wxFILE_SEP_PATH;
#endif

    // set default and alternative config names
    conf_filename = prefix + wxT("config");
    alt_conf_filename = prefix + wxT("alt-config");

    // set config file for options automatically saved
    // it will be accessed only via wxConfig::Get()
    wxConfig *config = new wxConfig(wxT(""), wxT(""), 
                                    prefix + wxT("wxoptions"), wxT(""), 
                                    wxCONFIG_USE_LOCAL_FILE);
    wxConfig::Set(config); 

    config_dir = get_user_conffile("configs") + wxFILE_SEP_PATH;
    if (!wxDirExists(config_dir))
        wxMkdir(config_dir);

    DataEditorDlg::read_transforms();

    // Create the main frame window
    frame = new FFrame(NULL, -1, wxT("fityk"), wxDEFAULT_FRAME_STYLE);

    wxConfigBase *cf;
    wxString g_config;
    if (cmdLineParser.Found(wxT("g"), &g_config))
        cf = new wxFileConfig(wxT(""), wxT(""), config_dir + g_config, 
                              wxT(""), wxCONFIG_USE_LOCAL_FILE);
    else
        cf = new wxConfig(wxT(""), wxT(""), conf_filename, wxT(""), 
                                    wxCONFIG_USE_LOCAL_FILE);
    frame->read_all_settings(cf);

    frame->plot_pane->set_mouse_mode(mmd_zoom);

    frame->Show(true);

    // it does not work earlier, problems with OutputWin colors (wxGTK gtk1.2)
    frame->io_pane->output_win->read_settings(cf);
    frame->io_pane->output_win->show_fancy_dashes();
    // sash inside wxNoteBook can have wrong position (eg. wxGTK 2.7.1)
    frame->sidebar->read_settings(cf);
    delete cf;

    SetTopWindow(frame);

    if (!cmdLineParser.Found(wxT("I"))) {
        // run initial commands (from ~/.fityk/init file)
        wxString startup_file = get_user_conffile(startup_commands_filename);
        if (wxFileExists(startup_file)) {
            ftk->get_ui()->exec_script(wx2s(startup_file));
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
        ftk->get_ui()->exec_and_log(wx2s(cmd));
    //the rest of parameters/arguments are scripts and/or data files
    vector<string> p;
    for (unsigned int i = 0; i < cmdLineParser.GetParamCount(); i++) 
        p.push_back(wx2s(cmdLineParser.GetParam(i)));
    if (cmdLineParser.Found(wxT("r")) && p.size() > 1) { // reorder
        sort(p.begin(), p.end(), less_filename(find_common_prefix_length(p)));
    }
    for (vector<string>::const_iterator i = p.begin(); i != p.end(); ++i) 
        ftk->get_ui()->process_cmd_line_filename(*i);
    if (ftk->get_ds_count() > 1) {
        frame->SwitchSideBar(true);
        // zoom to show all loaded datafiles
        ftk->view.parse_and_set(vector<string>(4), 
                                range_vector(0, ftk->get_ds_count())); 
    }
}

// search for `name' in two or three directories:  
//   wxStandardPaths::GetResourcesDir() 
//                        on Mac: appname.app/Contents/Resources bundle subdir
//                        on Win: dir where executable is
//   HELP_DIR = $(pkgdatadir), not defined on Win
//   {exedir}/../../doc/ - for uninstalled program
wxString get_full_path_of_help_file (wxString const& name)
{
    wxPathList paths;

    // installed path
#if defined(__WXMAC__) || defined(__WXMSW__)
    paths.Add(wxStandardPaths::Get().GetResourcesDir());
#endif
#ifdef HELP_DIR
    paths.Add(wxT(HELP_DIR));
#endif

    // uninstalled path, relative to executable
    paths.Add(wxPathOnly(wxGetApp().argv[0]) + wxFILE_SEP_PATH + wxT("..") 
              + wxFILE_SEP_PATH + wxT("..") + wxFILE_SEP_PATH + wxT("doc"));

    wxString path = paths.FindAbsoluteValidPath(name);
    if (path.IsEmpty())
        wxMessageBox(wxT("File ") + name + wxT(" was not found."), 
                     wxT("File not found."), wxICON_ERROR);
    return path; 
}


