/* vi:set ts=8 sts=8 sw=8 noet:
 *
 * PMS	<<Practical Music Search>>
 * Copyright (C) 2006-2015  Kim Tore Jensen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * pms.cpp - The PMS main class
 *
 */

#include "pms.h"

#include <mpd/client.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

/* Maximum time to spend waiting for events in main loop */
#define MAIN_LOOP_INTERVAL 1000

using namespace std;

Pms *		pms;


/*
 * 1..2..3..
 */
int main(int argc, char *argv[])
{
	int		exitcode;

	pms = new Pms(argc, argv);

	if (!pms) {
		printf("Not enough memory, aborting.\n");
		return PMS_EXIT_LOMEM;
	}

	exitcode = pms->init();

	if (exitcode == -1) {
		return PMS_EXIT_SUCCESS;
	} else if (exitcode == 0) {
		exitcode = pms->main();
	}

	delete pms;
	return exitcode;	
}

/**
 * Return the time difference between two timespec structs.
 */
struct timespec difftime(struct timespec start, struct timespec end)
{
	struct timespec temp;

	if ((end.tv_nsec - start.tv_nsec) < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}

	return temp;
}

/*
 * Init
 */
Pms::Pms(int c, char **v)
{
	argc = c;
	argv = v;
	disp = NULL;
}

/*
 * Unit
 */
Pms::~Pms()
{
}

struct timespec
Pms::get_clock()
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		perror("Failed to increase internal timer");
		abort();
	}

	return now;
}

/**
 * Poll stdin and the MPD socket for events.
 *
 * Returns true if any events occurred, false if not.
 */
bool
Pms::poll_events(long timeout_ms)
{
	struct timeval timeout;
	int mpd_fd;
	int nfds;
	int rc;

	if ((mpd_fd = conn->get_mpd_file_descriptor()) != -1) {
		FD_SET(mpd_fd, &poll_file_descriptors);
	}

	FD_SET(STDIN_FILENO, &poll_file_descriptors);

	nfds = STDIN_FILENO > mpd_fd ? STDIN_FILENO : mpd_fd;

	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms * 1000) - (timeout.tv_sec * 1000000);

	rc = select(++nfds, &poll_file_descriptors, NULL, NULL, &timeout);

	assert(rc != EINVAL);

	if (rc == -1) {
		FD_ZERO(&poll_file_descriptors);
	}

	return (rc > 0);
}

/**
 * Return true if there is data on the MPD socket. Remember to call
 * poll_events() beforehand.
 */
bool
Pms::has_mpd_events()
{
	return FD_ISSET(conn->get_mpd_file_descriptor(), &poll_file_descriptors);
}

/**
 * Return true if there is data on stdin. Remember to call poll_events()
 * beforehand.
 */
bool
Pms::has_stdin_events()
{
	return FD_ISSET(STDIN_FILENO, &poll_file_descriptors);
}

/**
 * Check if there is an MPD IDLE event on the MPD socket.
 * Set pending flags on the Control class, and sets real idle status.
 *
 * Returns true if there was an IDLE event, false if none.
 */
bool
Pms::run_has_idle_events()
{
	enum mpd_idle idle_reply;

	if (!has_mpd_events()) {
		return false;
	}

	pms->log(MSG_DEBUG, 0, "Received IDLE reply from server.\n");
	idle_reply = mpd_recv_idle(conn->h(), true);

	comm->set_mpd_idle_events(idle_reply);
	comm->set_is_idle(false);
	timer_elapsed = get_clock();

	return true;
}

/**
 * Check if currently playing song has changed since last call.
 *
 * Returns true if song has changed, false if not.
 */
bool
Pms::song_changed()
{
	Song * song;
	static song_t last_song_id = MPD_SONG_NO_ID;
	song_t current_song_id;
	bool rc;

	song = cursong();
	if (song) {
		current_song_id = song->id;
	} else {
		current_song_id = MPD_SONG_NO_ID;
	}

	rc = (current_song_id != last_song_id);

	last_song_id = current_song_id;

	return rc;
}

/**
 * Center the cursor on the currently playing song.
 */
void
Pms::run_cursor_follow_playback()
{
	pms_window * win;

	win = disp->findwlist(comm->activelist());
	if (!win) {
		return;
	}

	setwin(win);
	win->gotocurrent();
}

/*
 * Connection and main loop
 */
int
Pms::main()
{
	string			t_str;
	pms_pending_keys	pending = PEND_NONE;
	char			pass[512] = "";
	bool			songchanged = false;
	time_t			timer = 0;
	int			rc;
	bool need_init_follow_playback = true;

	/* Error codes returned from MPD */
	enum mpd_error		error;
	enum mpd_server_error	server_error;

	/* Connection */
	printf(_("Connecting to host %s, port %ld..."), options->get_string("host").c_str(), options->get_long("port"));

	if (conn->connect() != MPD_ERROR_SUCCESS)
	{
		printf(_("failed.\n"));
		printf("%s\n", mpd_connection_get_error_message(conn->h()));

		return PMS_EXIT_CANTCONNECT;
	}

	printf(_("connected.\n"));

	/* Password? */
	if (options->get_string("password").size() > 0)
	{
		printf(_("Sending password..."));
		if (comm->sendpassword(options->get_string("password"))) {
			printf(_("password accepted.\n"));
		} else {
			printf(_("wrong password.\n"));
			conn->clear_error();
		}
	}

	do {
		if (!comm->get_available_commands()) {
			printf(_("Failed to get a list of available commands, retrying...\n"));
			conn->clear_error();
			sleep(1);
			continue;
		}

		if (comm->authlevel() & AUTH_READ) {
			break;
		}

		printf(_("This mpd server requires a password.\n"));
		printf(_("Password: "));

		fgets(pass, 512, stdin) ? 1 : 0; //ternary here is a hack to get rid of a warn_unused_result warning
		if (pass[strlen(pass)-1] == '\n') {
			pass[strlen(pass)-1] = '\0';
		}

		options->set_string("password", pass);
		if (!comm->sendpassword(pass)) {
			printf(_("Wrong password, try again.\n"));
			conn->clear_error();
		}
	} while(true);

	printf(_("Successfully logged in.\n"));

	_shutdown = false;
	if (!disp->init())
	{
		printf(_("Can't initialize display!\n"));
		return PMS_EXIT_NODISPLAY;
	}

	/* Workaround for buggy ncurses clearing the screen on first getch() */
	//getch();

	/* Set up library and playlist windows */
	playlist = disp->create_playlist();
	library = disp->create_playlist();

	assert(playlist != NULL);
	assert(library != NULL);

	playlist->settitle(_("Playlist"));
	library->settitle(_("Library"));
	playlist->list = comm->playlist();
	library->list = comm->library();

	playlist->set_column_size();
	library->set_column_size();

	connect_window_list();

	/* Focus startup list */
	comm->activatelist(comm->playlist());
	t_str = options->get_string("startuplist");
	if (t_str == "library")
	{
		comm->activatelist(comm->library());
	}
	else if (t_str.size() > 0 && t_str != "playlist")
	{
		comm->activatelist(comm->find_playlist(t_str));
	}
	disp->activate(disp->findwlist(comm->activelist()));

	disp->forcedraw();
	disp->refresh();

	/* Reset all clocks */
	timer_now = get_clock();
	timer_elapsed = get_clock();

	/*
	 * Main loop
	 * FIXME: reduce the size of this behemoth
	 */
	do
	{
		/* Set timer */
		timer_now = get_clock();

		/* For debugging the main loop */
		log(MSG_DEBUG, 0, "--> Main loop iteration, clock = %ld.%ld\n", timer_now.tv_sec, timer_now.tv_nsec);

		/* Test if some error has occurred */
		if ((error = mpd_connection_get_error(conn->h())) != MPD_ERROR_SUCCESS) {

			log(MSG_STATUS, STERR, "MPD error: %s", mpd_connection_get_error_message(conn->h()));

			/* Try to recover from error. If the error is
			 * non-recoverable, reconnect to the MPD server.
			 */
			if (!mpd_connection_clear_error(conn->h())) {

				/* FIXME: gradually increase connection attempts? */
				/* FIXME: use reconnectdelay setting */
				/* FIXME: separate thread */
				sleep(1);
				conn->connect();
				continue;
			}
		}

		/* Increase time elapsed. */
		if (comm->status()->state == MPD_STATE_PLAY) {
			timer_tmp = difftime(timer_elapsed, timer_now);
			comm->status()->increase_time_elapsed(timer_tmp);
			disp->topbar->wantdraw = true;
		}
		timer_elapsed = get_clock();

		/* Run any pending updates */
		if (!comm->run_pending_updates()) {
			log(MSG_DEBUG, 0, "Failed running pending updates, MPD error follows in next main loop iteration\n");
			continue;
		}

		/* Library updates triggers re-calculation of column sizes,
		 * triggers draw, etc. */
		/* FIXME: move responsibilities? */
		if (comm->has_finished_update(MPD_IDLE_DATABASE)) {
			log(MSG_STATUS, STOK, _("Library has been updated."));
			disp->actwin()->wantdraw = true;
			library->list->sort(options->get_string("sort"));
			library->set_column_size();
			connect_window_list();
			comm->clear_finished_update(MPD_IDLE_DATABASE);
		}

		/* Draw XTerm window title when state is updated */
		if (comm->has_any_finished_updates()) {
			disp->topbar->wantdraw = true;
			disp->actwin()->wantdraw = true;
			disp->set_xterm_title();
		}

		/* Playlist updates triggers re-calculation of column sizes,
		 * triggers draw, etc. */
		/* FIXME: move responsibilities? */
		if (comm->has_finished_update(MPD_IDLE_PLAYLIST)) {
			playlist->set_column_size();
			comm->clear_finished_update(MPD_IDLE_PLAYLIST);
		}

		/* Draw statusbar and topbar on player update. */
		if (comm->has_finished_update(MPD_IDLE_PLAYER)) {

			/* Shell command when song finishes */
			/* FIXME: move into separate function */
			if (comm->status()->state == MPD_STATE_STOP && pending != PEND_STOP) {
				if (options->get_string("onplaylistfinish").size() > 0 && cursong() && cursong()->pos == comm->playlist()->end()) {
					log(MSG_CONSOLE, STOK, _("Reached end of playlist, running automation command: %s"), options->get_string("onplaylistfinish").c_str());
					int code = system(options->get_string("onplaylistfinish").c_str());
				}
			}

			/* Execute 'cursor follows playback'. */
			if (song_changed() && (need_init_follow_playback || options->get_bool("followplayback"))) {
				run_cursor_follow_playback();
				need_init_follow_playback = false;
			}

			drawstatus();
			comm->clear_finished_update(MPD_IDLE_PLAYER);
		}

		/* Create windows containing MPD playlists. */
		if (comm->has_finished_update(MPD_IDLE_STORED_PLAYLIST)) {
			connect_window_list();
			comm->clear_finished_update(MPD_IDLE_STORED_PLAYLIST);
		}

		/* Reset status */
		if (needs_statusbar_reset()) {
			drawstatus();
		}

		/* Redraw the screen. */
		/* FIXME: where to put this? */
		if (mediator->changed("redraw")) {
			disp->forcedraw();
		} else {
			disp->draw();
		}
		disp->refresh();


		/**
		 * Start IDLE mode and polling. Keep this code at the end of
		 * the main loop.
		 */

		/* Ensure that we are in IDLE mode. */
		if (!comm->is_idle()) {
			if (!comm->idle()) {
				continue;
			}
		}

		/* Block until events received or timeout reached. */
		poll_events(MAIN_LOOP_INTERVAL);

		/* Process events from the IDLE socket. */
		if (run_has_idle_events()) {
			continue;
		}

		/* Process events from the input socket. */
		if (has_stdin_events()) {
			input->get_keystroke();
			pending = input->dispatch();
			if (pending != PEND_NONE) {
				handle_command(pending);
			}
		}

		/* Progress to next song if applicable, and make sure we are
		 * synched with IDLE events before doing it. */
		if (!comm->has_pending_updates()) {
			progress_nextsong();
		}

		/* Check out mediator events */
		/* FIXME: implement this functionality with ZeroMQ */
		if (mediator->changed("setting.sort"))
			comm->library()->sort(options->get_string("sort"));
		else if (mediator->changed("setting.ignorecase"))
			comm->library()->sort(options->get_string("sort"));
		else if (mediator->changed("setting.columns"))
			disp->actwin()->set_column_size();
		else if (mediator->changed("setting.mouse"))
			disp->setmousemask();
		else if (mediator->changed("redraw.topbar"))
			disp->resized();
		else if (mediator->changed("topbarvisible"))
			disp->resized();
		else if (mediator->changed("topbarborders"))
			disp->resized();
		else if (mediator->changed("topbarspace"))
			disp->resized();
		else if (mediator->changed("columnspace"))
			disp->resized();
		else if (mediator->changed("setting.topbarclear"))
		{
			if (options->get_bool("topbarclear"))
				options->topbar.clear();
		}
	}
	while (!_shutdown);

	log(MSG_CONSOLE, STOK, _("Shutting down program.\n"));

	delete disp;
	delete comm;
	delete conn;

	/* Unclutter the prompt */
	printf("\n");

	return PMS_EXIT_SUCCESS;
}

/*
 * Set up neccessary variables
 */
int			Pms::init()
{
	string			str;
	vector<string> *	tok;

	int			exitcode;
	char *			host;
	char *			port;
	char *			password;
	const char *		charset = NULL;
	
	/* Internal pointers */
	msg = new Message();
	mediator = new Mediator();
	interface = new Interface();
	formatter = new Formatter();

	/* Setup locales and internationalization */
	setlocale(LC_ALL, "");
	setlocale(LC_CTYPE, "");
	g_get_charset(&charset);
	bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, charset);
	textdomain(GETTEXT_PACKAGE);

	/* Print program header */
	printf("%s v%s\n%s\n", PMS_NAME, PACKAGE_VERSION, PMS_COPYRIGHT);

	/* Read important environment variables */
	host = getenv("MPD_HOST");
	port = getenv("MPD_PORT");
	password = getenv("MPD_PASSWORD");

	/* Set up field types */
	fieldtypes = new Fieldtypes();
	fieldtypes->add("num", _("#"), FIELD_NUM, 0, NULL);
	fieldtypes->add("file", _("Filename"), FIELD_FILE, 0, sort_compare_file);
	fieldtypes->add("artist", _("Artist"), FIELD_ARTIST, 0, sort_compare_artist);
	fieldtypes->add("artistsort", _("Artist sort name"), FIELD_ARTISTSORT, 0, sort_compare_artistsort);
	fieldtypes->add("albumartist", _("Album artist"), FIELD_ALBUMARTIST, 0, sort_compare_albumartist);
	fieldtypes->add("albumartistsort", _("Album artist sort name"), FIELD_ALBUMARTISTSORT, 0, sort_compare_albumartistsort);
	fieldtypes->add("title", _("Title"), FIELD_TITLE, 0, sort_compare_title);
	fieldtypes->add("album", _("Album"), FIELD_ALBUM, 0, sort_compare_album);
	fieldtypes->add("track", _("Track"), FIELD_TRACK, 6, sort_compare_track);
	fieldtypes->add("trackshort", _("No"), FIELD_TRACKSHORT, 3, sort_compare_track);
	fieldtypes->add("length", _("Length"), FIELD_TIME, 7, sort_compare_length);
	fieldtypes->add("date", _("Date"), FIELD_DATE, 11, sort_compare_date);
	fieldtypes->add("year", _("Year"), FIELD_YEAR, 5, sort_compare_year);
	fieldtypes->add("name", _("Name"), FIELD_NAME, 0, sort_compare_name);
	fieldtypes->add("genre", _("Genre"), FIELD_GENRE, 0, sort_compare_genre);
	fieldtypes->add("composer", _("Composer"), FIELD_COMPOSER, 0, sort_compare_composer);
	fieldtypes->add("performer", _("Performer"), FIELD_PERFORMER, 0, sort_compare_performer);
	fieldtypes->add("disc", _("Disc"), FIELD_DISC, 5, sort_compare_disc);
	fieldtypes->add("comment", _("Comment"), FIELD_COMMENT, 0, sort_compare_comment);

	/* Set up default bindings */
	if (!init_commandmap())
	{
		return PMS_EXIT_NOCOMMAND;
	}
	options = new Options();
	init_default_keymap();

	/* Our configuration */
	config = new Configurator(options, bindings);

	/* Some default options */
	options->set_string("host", (host ? host : "127.0.0.1"));
	if (!password && host)
	{
		tok = splitstr(host, "@");
		if (tok->size() == 2)
		{
			options->set_string("host", (*tok)[0]);
			options->set_string("password", (*tok)[1]);
		}
		delete tok;
	}
	if (options->get_string("password").size() == 0)
	{
		options->set_string("password", (password ? password : ""));
	}
	options->set_long("port", (port ? atoi(port) : 6600));

	if (options->get_long("port") <= 0 || options->get_long("port") > 65535)
	{
		printf(_("Error: port number in environment variable MPD_PORT must be from 1-65535\n"));
		return PMS_EXIT_BADARGS;
	}

	/* Parse command-line */
	if ((exitcode = parse_args(argc, argv)) != PMS_EXIT_SUCCESS) {
		return exitcode;
	}

	if (!config->loadconfigs()) {
		return PMS_EXIT_CONFIGERR;
	}

	/* Seed random number generator */
	srand(time(NULL));

	/* Setup some important stuff */
	conn	= new Connection(options->get_string("host"), options->get_long("port"), options->get_long("mpd_timeout") * 1000);
	comm	= new Control(conn);
	disp	= new Display(comm);
	input	= new Input();
	if (!conn || !comm || !disp || !input)
		return PMS_EXIT_LOMEM;

	/* Initialization finished */
	return PMS_EXIT_SUCCESS;
}






/*
 * Converts long to string
 */
string			Pms::tostring(long number)
{
	ostringstream s;
	s << number;
	return s.str();
}

/*
 * Converts size_t to string
 */
string			Pms::tostring(size_t number)
{
	ostringstream s;
	s << number;
	return s.str();
}

/*
 * Converts int to string
 */
string			Pms::tostring(int number)
{
	ostringstream s;
	s << number;
	return s.str();
}

/**
 * Convert a const char * to string
 */
string
Pms::tostring(const char *src)
{
	return src ? src : "";
}

/*
 * Split a string into tokens
 */
vector<string> *	Pms::splitstr(string str, string delimiter)
{
	vector<string> *	tokens = new vector<string>;

	string::size_type last	= str.find_first_not_of(delimiter, 0);
	string::size_type pos	= str.find_first_of(delimiter, last);

	while (string::npos != pos || string::npos != last)
	{
		tokens->push_back(str.substr(last, pos - last));
		last = str.find_first_not_of(delimiter, pos);
		pos = str.find_first_of(delimiter, last);
	}
	
	return tokens;
}

/*
 * Join tokens into a string
 */
string			Pms::joinstr(vector<string> * source, vector<string>::iterator start, vector<string>::iterator end, string delimiter)
{
	string			dest = "";

	while (start != source->end())
	{
		dest += *start;

		if (start == end)
			break;

		if (++start != end)
		{
			dest += delimiter;
		}
	}
	
	return dest;
}

/*
 * Formats seconds into the format Dd H:MM:SS.
 */
string			Pms::timeformat(int seconds)
{
	static const int	day	= (60 * 60 * 24);
	static const int	hour	= (60 * 60);
	static const int	minute	= 60;

	int		i;
	string		s = "";

	/* No time */
	if (seconds < 0)
	{
		s = "--:--";
		return s;
	}

	/* days */
	if (seconds >= day)
	{
		i = seconds / day;
		s = Pms::tostring(i) + "d ";
		seconds %= day;
	}

	/* hours */
	if (seconds >= hour)
	{
		i = seconds / hour;
		s += zeropad(i, 1) + ":";
		seconds %= hour;
	}

	/* minutes */
	i = seconds / minute;
	s = s + zeropad(i, 2) + ":";
	seconds %= minute;

	/* seconds */
	s += zeropad(seconds, 2);

	return s;
}

/*
 * Return "song" or "songs" based on plural or not
 */
string			Pms::pluralformat(unsigned int i)
{
	if (i == 1)
		return _("song");
	else
		return _("songs");
}
/*
 * Pad integer with zeroes up to target length
 */
string			Pms::zeropad(int i, unsigned int target)
{
	string s;
	s = Pms::tostring(i);
	while(s.size() < target)
		s = '0' + s;
	return s;
}

/*
 * Replaces % with %%
 */
string			Pms::formtext(string text)
{
	string::const_iterator	i;
	string			nutext;

	i = text.begin();
	nutext.clear();

	while (i != text.end())
	{
		nutext += *i;
		if (*i == '%')
			nutext += *i;
		++i;
	}

	return nutext;
}

/*
 * Return true if the terminal supports Unicode
 */
bool			Pms::unicode()
{
	const char *		charset = NULL;

	g_get_charset(&charset);
	return strcmp(charset, "UTF-8") == 0;
}










/*
 * Run a shell command
 *
 * FIXME: perhaps this command should be within Interface class?
 * TODO: add %artist% tags through the field pattern parser: meaning %file% -> filename, not % -> filename
 *	...but current implementation is nice and vim-like
 */
bool			Pms::run_shell(string cmd)
{
	string				search;
	string				replace;
	string::size_type		pos;
	int				i;
	Songlist *			list;
	char				c;

	msg->clear();

	/*
	 * %: path to current song, not enclosed in quotes
	 */
	if (cursong())
	{
		search = "%";
		replace = options->get_string("libraryroot");
		replace += cursong()->file;
		pos = 0;
		while ((pos = cmd.find(search, pos)) != string::npos)
		{
			if (pos == 0 || cmd[pos - 1] != '\\')
				cmd.replace(pos, search.size(), replace);
			pos++;
		}
	}

	/*
	 * ##: path to each song in selection (or each song on the current 
	 * playlist if there is no selection), each enclosed with doublequotes 
	 * and separated by spaces
	 */
	list = disp->actwin()->plist();
	search = "##";
	if (cmd.find(search, 0) != string::npos && list && list->size())
	{
		replace = "";
		for (i = 0; i < list->size(); i++)
		{
			if (!list->selection.size || list->song(i)->selected)
			{
				replace += options->get_string("libraryroot");
				replace += list->song(i)->file;
				replace += "\" \"";
			}
		}
		if (replace.size() > 0)
		{
			replace = "\"" + replace.substr(0, replace.size() - 2);
			pos = 0;
			while ((pos = cmd.find(search, pos)) != string::npos)
			{
				if (pos == 0 || cmd[pos - 1] != '\\')
					cmd.replace(pos, search.size(), replace);
				pos++;
			}
		}
	}

	/*
	 * #: path to song the cursor is on, not enclosed in quotes
	 */
	if (disp->cursorsong())
	{
		search = "#";
		replace = options->get_string("libraryroot");
		replace += disp->cursorsong()->file;
		pos = 0;
		while ((pos = cmd.find(search, pos)) != string::npos)
		{
			if (pos == 0 || cmd[pos - 1] != '\\')
				cmd.replace(pos, search.size(), replace);
			pos++;
		}
	}

	//pms->log(MSG_DEBUG, 0, "running shell command '%s'\n", cmd.c_str());
	endwin();

	msg->code = system(cmd.c_str());
	msg->code = WEXITSTATUS(msg->code);

	pms->log(MSG_DEBUG, 0, "Shell returned %d\n", msg->code);
	if (msg->code != 0)
		printf(_("\nShell returned %d\n"), msg->code);

	printf(_("\nPress ENTER to continue"));
	fflush(stdout);
	{
		/* soak up return value to suppress warning */
		int key = scanf("%c", &c);
	}

	reset_prog_mode();
	refresh();

	return true;
}

/*
 * Returns the currently playing song
 */
Song *			Pms::cursong()
{
	assert(comm != NULL);
	return comm->song();
}

/* 
 * Reset status to its natural state.
 */
void
Pms::drawstatus()
{
	if (input->mode() == INPUT_JUMP) {
		log(MSG_STATUS, STOK, "/%s", formtext(input->text).c_str());
	} else if (input->mode() == INPUT_FILTER) {
		log(MSG_STATUS, STOK, ":g/%s", formtext(input->text).c_str());
	} else if (input->mode() == INPUT_COMMAND) {
		log(MSG_STATUS, STOK, ":%s", formtext(input->text).c_str());
	} else {
		log(MSG_STATUS, STOK, "%s", playstring().c_str());
	}

	/* Do not redraw statusbar anymore */
	timer_statusbar.tv_sec = 0;
	timer_statusbar.tv_nsec = 0;
}

/**
 * Determine whether the statusbar text should be reset to its natural state.
 *
 * Returns true if the statusbar is due for an update, false if not.
 */
bool
Pms::needs_statusbar_reset()
{
	/* Check if redraw is disabled */
	if (timer_statusbar.tv_sec == 0 && timer_statusbar.tv_nsec == 0) {
		return false;
	}

	timer_tmp = difftime(timer_statusbar, timer_now);
	return (timer_tmp.tv_sec >= options->get_long("resetstatus"));
}

/*
 * Return a textual description on how song progression works.
 *
 * FIXME: this function is a mess. De-duplicate and use common code for this
 * function and progress_nextsong().
 */
string
Pms::playstring()
{
	string		s;
	string		list_name = "<unknown>";
	bool		is_last_in_playlist;
	bool		playlist_is_active;
	bool		library_is_active;
	Mpd_status *	status;

	status = comm->status();

	assert(status != NULL);

	if (!conn->connected()) {
		s = "Not connected.";
		return s;
	}

	if (status->state == MPD_STATE_STOP || !cursong()) {
		s = "Stopped.";
		return s;
	}

	if (status->state == MPD_STATE_PAUSE) {
		s = "Paused...";
		return s;
	}

	if (comm->activelist()) {
		list_name = comm->activelist()->filename;
	}

	playlist_is_active = (comm->activelist() == comm->playlist());
	library_is_active = (comm->activelist() == comm->library());

	/* FIXME: playlist should give the correct name in a name() function */
	if (list_name.size() == 0) {
		if (playlist_is_active) {
			list_name = "playlist";
		} else if (library_is_active) {
			list_name = "library";
		}
	}

	s = "Playing ";

	if (status->consume) {
		s += "and consuming ";
	}

	if (status->random) {
		s += "random songs from playlist.";
		return s;
	}

	if (status->single) {
		if (status->repeat && !status->consume) {
			s += "the current song repeatedly.";
		} else {
			s += "this song, then stopping.";
		}
		return s;
	}

	/* FIXME: separate function? */
	is_last_in_playlist = (cursong()->pos == static_cast<song_t>(comm->playlist()->end()));

	if (status->repeat) {
		s += "songs from playlist repeatedly.";
		return s;
	}

	if (playlist_is_active) {
		if (is_last_in_playlist) {
			s += "this song, then stopping.";
		} else {
			s += "songs from playlist.";
		}
		return s;
	}

	if (is_last_in_playlist) {
		s += "this song, then ";
		if (!status->repeat && playlist_is_active) {
			s += "stopping.";
			return s;
		}
	}

	if (!is_last_in_playlist) {
		if (!status->consume) {
			s += "through ";
		}
		s += "playlist, then ";
	}

	if (!playlist_is_active && options->get_bool("followcursor")) {
		s += "following cursor.";
		return s;
	}

	s += "songs from " + list_name + ".";

	return s;
}

/*
 * Put an arbitrary message into the message log
 */
void			Pms::putlog(Message * m)
{
	if (m->code == 0 && m->str.size() == 0)
		return;

	log(MSG_CONSOLE, m->code, m->str.c_str());
}

/*
 * Log a message.
 * Verbosity levels:
 *  0 = statusbar
 *  1 = console
 *  2 = debug
 */
void
Pms::log(int verbosity, long code, const char * format, ...)
{
	long		loglines;
	va_list		ap;
	char		buffer[1024];
	char		tbuffer[20];
	string		level;
	Message *	m;
	tm *		timeinfo;
	color *		pair;

	if (verbosity >= MSG_DEBUG && !pms->options->get_bool("debug"))
		return;

	m = new Message();
	if (m == NULL)
		return;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	m->str = buffer;
	m->code = code;

	if (verbosity == MSG_STATUS)
	{
		m->str += "\n";

		if (code == STOK)
			pair = options->colors->status;
		else
			pair = options->colors->status_error;

		disp->statusbar->clear(false, pair);
		colprint(disp->statusbar, 0, 0, pair, "%s", buffer);
		timer_statusbar = get_clock();
		disp->refresh();
	}

	if (verbosity <= MSG_DEBUG && pms->options->get_bool("debug"))
	{
		timeinfo = localtime(&(m->timestamp));
		strftime(tbuffer, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
		if (verbosity == MSG_STATUS)
			level = "status";
		else if (verbosity == MSG_CONSOLE)
			level = "console";
		else if (verbosity == MSG_DEBUG)
			level = "debug";
		fprintf(stderr, "%s /%s/ %s", tbuffer, level.c_str(), m->str.c_str());
	}

	if (!disp && verbosity < MSG_DEBUG)
	{
		printf("%s", buffer);
	}

	msglog.push_back(m);
	loglines = options->get_long("msg_buffer_size");
	if (loglines > 0 && msglog.size() > loglines)
		msglog.erase(msglog.begin());
}

/*
 * Checks if time is right for song progression, and takes necessary action.
 *
 * FIXME: split into two functions
 * FIXME: dubious return value
 */
bool			Pms::progress_nextsong()
{
	static song_t		last_song_id = MPD_SONG_NO_ID;
	static Song *		lastcursor = NULL;
	Songlist *		list = NULL;
	unsigned int		song_time_remaining;
	Mpd_status *		status = comm->status();

	/* No song progression without an active song, probably meaning that
	 * the player is stopped or something is wrong. */
	if (!cursong()) {
		return false;
	}

	/* No song progression if not playing. */
	if (status->state != MPD_STATE_PLAY) {
		return false;
	}

	/* If the active list is the playlist, PMS doesn't need to do anything,
	 * because MPD handles the rest. */
	list = comm->activelist();
	assert(list != NULL);
	if (list == comm->playlist()) {
		return false;
	}

	/* Only add songs when there the currently playing song is near the end. */
	song_time_remaining = status->time_total - status->time_elapsed - status->crossfade;
	if (song_time_remaining > options->get_long("nextinterval")) {
		return false;
	}

	/* No auto-progression in single mode */
	if (status->single) {
		return false;
	}

	/* Defeat desync with server */
	last_song_id = cursong()->id;

	/* Normal progression: reached end of playlist */
	if (cursong()->pos == static_cast<int>(playlist->list->end())) { 

		pms->log(MSG_DEBUG, 0, "Auto-progressing to next song.\n");

		/* Playback follows cursor */
		if (options->get_bool("followcursor") && lastcursor != disp->cursorsong() && disp->cursorsong()->file != cursong()->file)
		{
			pms->log(MSG_DEBUG, 0, "Playback follows cursor: last cursor=%p, now cursor=%p.\n", lastcursor, disp->cursorsong());
			lastcursor = disp->cursorsong();
			last_song_id = comm->add(comm->playlist(), lastcursor);
		}

		/* Normal song progression */
		last_song_id = playnext(false);
	}

	if (lastcursor == NULL) {
		lastcursor = disp->cursorsong();
	}

	return (last_song_id != MPD_SONG_NO_ID);
}

/*
 * Create new windows for each custom playlist
 *
 * FIXME: wtf, why?
 */
bool
Pms::connect_window_list()
{
	pms_window *			win;
	vector<Playlist *>::iterator	i;

	i = comm->playlists.begin();
	while (i != comm->playlists.end()) {
		if (disp->findwlist(*i) == NULL) {
			win = disp->create_playlist();
			assert(win != NULL);
			win->setplist(*i);
		}
		++i;
	}

	return true;
}

/*
 * Default key bindings
 */
void			Pms::init_default_keymap()
{
	bindings->clear();

	/* Movement */
	bindings->add("up", "move-up");
	bindings->add("down", "move-down");
	bindings->add("pageup", "move-pgup");
	bindings->add("pagedown", "move-pgdn");
	bindings->add("^B", "move-pgup");
	bindings->add("^F", "move-pgdn");
	bindings->add("^U", "move-halfpgup");
	bindings->add("^D", "move-halfpgdn");
	bindings->add("^Y", "scroll-up");
	bindings->add("^E", "scroll-down");
	bindings->add("z", "center-cursor");
	bindings->add("home", "move-home");
	bindings->add("end", "move-end");
	bindings->add("g", "goto-current");
	bindings->add("R", "goto-random");
	bindings->add("j", "move-down");
	bindings->add("k", "move-up");
	bindings->add("t", "prev-window");
	bindings->add("T", "next-window");
	bindings->add("(", "prev-of album");
	bindings->add(")", "next-of album");
	bindings->add("{", "prev-of artist");
	bindings->add("}", "next-of artist");
	bindings->add("1", "change-window playlist");
	bindings->add("2", "change-window library");
	bindings->add("w", "change-window windowlist");
	// TODO: add this for a later version
	//bindings->add("W", "change-window directorylist");
	bindings->add("tab", "last-window");

	/* Searching */
	bindings->add("/", "quick-find");
	bindings->add("n", "next-result");
	bindings->add("N", "prev-result");

	/* Playlist management */
	bindings->add("a", "add");
	bindings->add("A", "add-to");
	bindings->add("b", "add-album");
	bindings->add("B", "play-album");
	bindings->add("delete", "remove");
	bindings->add("C", "crop");
	bindings->add("insert", "toggle-select");
	bindings->add("F12", "activate-list");
	bindings->add("^X", "delete-list");
	bindings->add("J", "move 1");
	bindings->add("K", "move -1");

	/* Controls */
	bindings->add("return", "play");
	bindings->add("kpenter", "play");
	bindings->add("backspace", "stop");
	bindings->add("p", "pause");
	bindings->add("space", "toggle-play");
	bindings->add("l", "next");
	bindings->add("h", "prev");
	bindings->add("M", "mute");
	bindings->add("r", "repeat");
	bindings->add("z", "random");
	bindings->add("c", "consume");
	bindings->add("s", "single");
	bindings->add("+", "volume +5");
	bindings->add("-", "volume -5");
	bindings->add("left", "seek -5");
	bindings->add("right", "seek 5");

	/* Maintenance */
	bindings->add("f", "toggle followcursor");
	bindings->add("F", "toggle followplayback");
	bindings->add("^F", "toggle followwindow");
	bindings->add(":", "command-mode");
	bindings->add("u", "update-library");
	bindings->add("v", "version");
	bindings->add("q", "quit");
	bindings->add("F1", "help");
	bindings->add("^L", "redraw");
}










/*
 * Print the version string
 */
void
Pms::print_version()
{
	printf("Uses libmpdclient (c) 2003-2015 The Music Player Daemon Project.\n");
	printf("This program is licensed under the GNU General Public License version 3.\n");
}

/*
 * Print switch usage
 */
void
Pms::print_usage()
{
	printf("Usage:\n");
	printf("  -%s\t\t\t%s\n", "v", "print version and exit");
	printf("  -%s\t\t\t%s\n", "h", "display this help screen and exit");
	printf("  -%s\t\t\t%s\n", "d", "turn on debugging to standard error");
	printf("  -%s\t\t%s\n", "c <filename>", "use an alternative config file");
	printf("  -%s\t\t%s\n", "H <host>", "connect to this MPD server");
	printf("  -%s\t\t%s\n", "p <port>", "connect to this port");
	printf("  -%s\t\t%s\n", "P <password>", "give this password to MPD server");
}

/*
 * Parse command-line arguments
 */
int
Pms::parse_args(int argc, char ** argv)
{
	int c;

	while ((c = getopt(argc, argv, "hdvc:H:p:P:")) != -1) {
		switch(c) {
			case 'd':
				options->set_bool("debug", true);
				break;
			case 'v':
				print_version();
				return -1;
			case 'h':
				print_usage();
				return -1;
			case 'c':
				options->set_string("configfile", optarg);
				break;
			case 'H':
				options->set_string("host", optarg);
				break;
			case 'p':
				options->set_long("port", atoi(optarg));
				if (options->get_long("port") <= 0 || options->get_long("port") > 65535) {
					printf(_("Error: port number must be from 1-65535\n"));
					return PMS_EXIT_BADARGS;
				}
				break;
			case 'P':
				options->set_string("password", optarg);
				break;
			case '?':
				if (optopt == 'c' || optopt == 'H' || optopt == 'p' || optopt == 'P') {
					printf(_("Error: option -%c requires an argument.\n"), optopt);
				} else {
					printf(_("Error: unknown option -%c.\n"), c);
				}
				print_usage();
				return PMS_EXIT_BADARGS;
			default:
				abort();
		}
	}

	return PMS_EXIT_SUCCESS;
}
