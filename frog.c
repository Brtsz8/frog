
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

#define FRAME_TIME	25
#define PASS_TIME	20
#define MVB_FACTOR	2							// move every FRAME_TIME * MVB_FACTOR [ms] BALL
#define MVC_FACTOR	5	

#define QUIT_TIME	3
#define QUIT		'k'
#define NOKEY		' '

#define BORDER		1
#define DELAY_ON	1
#define DELAY_OFF	0

#define MAIN_COLOR      1
#define STAT_COLOR      2
#define PLAY_COLOR      3
#define FROG_COLOR		4
#define CAR_COLOR		5

#define MAX_LVL 3

#define ENTER 10
// playwin (WIN) parameters
#define ROWS		26							
#define COLS		60
#define OFFY		4
#define OFFX		8

//random number generation 
#define RA(min, max) ( (min) + rand() % ((max) - (min) + 1) )

///////////////////////////////////
////////////Structs///////////////
/////////////////////////////////

// window structure
typedef struct {
	WINDOW* window;								
	int x, y;
	int rows, cols;
	int color;
} WIN;	

// moving object structure inside win(dow)
typedef struct {
	WIN* win;
	int color;								// normal color
	int bflag;								// background color flag = 1 (window), = 0 (own)
	int mv;									// move factor
	int x, y; 								// top-left corner
	int width, height;							// sizes
	int xmin, xmax;								// min/max -> place for moving in win->window
	int ymin, ymax;
	char** shape;								// shape of the object (2-dim characters array (box))
} OBJ;

typedef struct {
	unsigned int frame_time;
	float pass_time;
	int frame_no;
} TIMER;

//////////////////////////////////
//////////////Functions//////////
////////////////////////////////

WINDOW* Start()
{
	WINDOW* win;
	if ( (win = initscr()) == NULL ) {					// initialize ncurses
		fprintf(stderr, "Error initialising ncurses.\n");
		exit(EXIT_FAILURE);
    	}

	start_color();								// initialize colors
	init_pair(MAIN_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(PLAY_COLOR, COLOR_BLUE, COLOR_GREEN);
	init_pair(STAT_COLOR, COLOR_WHITE, COLOR_BLUE);
	init_pair(FROG_COLOR, COLOR_WHITE,COLOR_GREEN);
	init_pair(CAR_COLOR, COLOR_WHITE,COLOR_BLACK );

	
	noecho();		//NIE wypisuje inputu na ekran 
	curs_set(0);
	return win;
}

void CleanWin(WIN* W, int bo)							
{
	int i, j;
	wattron(W->window,COLOR_PAIR(W->color));
	if (bo) box(W->window,0,0);
	for(i = bo; i < W->rows - bo; i++)
		for(j = bo; j < W->cols - bo; j++)
			mvwprintw(W->window,i,j," ");
}
void Welcome(WINDOW* win)	
{
	// screen with - press any key to continue
	mvwaddstr(win, 1, 1, "Do you want to play a game?");
	mvwaddstr(win, 2, 1, "Press any key to continue..");
	wgetch(win);
	// clear next refresh)								
	wclear(win);								
	wrefresh(win);

}

int Menu(WIN* win)
{
	char choices[3][20] = {"level 1","level 2","level 3"};

	int move;
	int current = 0;

	while(1)
	{		
		for(int i = 0; i < MAX_LVL; i++)
		{
			if(i == current)
				wattron(win->window,A_REVERSE);
			mvwprintw(win->window,i+1,1,choices[i]);
			wattroff(win->window,A_REVERSE);
		}
		wrefresh(win->window);
		move = wgetch(win->window);

		switch(move)
		{
			case 'w':
				current--;
				if(current==-1)
					current=0;
				break;
			case 's':
				current++;
				if(current==MAX_LVL)
					current=MAX_LVL-1;
				break;
			default:	
				break;
		}
		if(move == ENTER)	break;
	}

	CleanWin(win,1);						
	wrefresh(win->window);

	return current +1 ;  //returns the level index
}

void Print(OBJ* ob)
{
	for(int i = 0; i < ob->height; i++)
		mvwprintw(ob->win->window, ob->y+i, ob->x,ob->shape[i]);
}

										// common function for both: Catcher and Ball
void Show(OBJ* ob, int dx, int dy)		// move: +-1 in both directions: dy,dx: -1,0,1
{
	char *sw = (char*)malloc(sizeof(char) * ob->width);
	memset(sw,' ',ob->width);

	wattron(ob->win->window,COLOR_PAIR(ob->color));
	

	if ((dy == 1) && (ob->y + ob->height < ob->ymax))
	{
		ob->y += dy;
		mvwprintw(ob->win->window, ob->y-1, ob->x,sw);
	}
	if ((dy ==-1) && (ob->y > ob->ymin))
	{
		ob->y += dy;
		mvwprintw(ob->win->window, ob->y+ob->height, ob->x,sw);
	}

	if ((dx == 1) && (ob->x + ob->width < ob->xmax))
	{
		ob->x += dx;
		for(int i = 0; i < ob->height; i++) mvwprintw(ob->win->window, ob->y+i, ob->x-1," ");
	}
	if ((dx ==-1) && (ob->x > ob->xmin))
	{
		ob->x += dx;
		for(int i = 0; i < ob->height; i++) mvwprintw(ob->win->window, ob->y+i, ob->x+ob->width," ");
	}

	Print(ob);

	if (ob->bflag) wattron(ob->win->window,COLOR_PAIR(ob->win->color));
	wrefresh(ob->win->window);
}

WIN* Init(WINDOW* parent, int rows, int cols, int y, int x, int color, int bo, int delay)
{
	WIN* W = (WIN*)malloc(sizeof(WIN));					
	W->x = x; W->y = y; W->rows = rows; W->cols = cols; W->color = color;
	W->window = subwin(parent, rows, cols, y, x);
	CleanWin(W, bo);
	if (delay == 0)  nodelay(W->window,TRUE);							
	// non-blocking reading of characters (for real-time game)
	wrefresh(W->window);
	return W;
}

void InitPos(OBJ* ob, int xs, int ys)
{
	ob->x = xs;
	ob->y = ys;
}

void EndGame(const char* info, WIN* W)						// sth at the end
{
	CleanWin(W,1);
	for(int i = QUIT_TIME; i > 0; i--)
	{
		mvwprintw(W->window,1,2,"%s Closing the game in %d seconds...",info,i);
		wrefresh(W->window);
		sleep(1);
	}
}

OBJ* InitFrog(WIN* w, int col)
{

	OBJ* ob	   = (OBJ*)malloc(sizeof(OBJ));						// C
	ob->bflag  = 1;									// normal colors (initially)
	ob->color  = col;
	ob->win    = w;
	ob->width  = 2;
	ob->height = 2;
	ob->mv     = 0;

	ob->shape = (char**)malloc(sizeof(char*) * ob->height);				// array of pointers (char*)
	for(int i = 0; i < ob->height; i++)
		ob->shape[i] = (char*)malloc(sizeof(char) * (ob->width + 1));		// +1: end-of-string (C): '\0'

	strcpy(ob->shape[0],"@@");
	strcpy(ob->shape[1],"XX");



	InitPos(ob,(ob->win->cols - ob->width) / 2,(ob->win->rows - ob->height) - 1 );
	ob->xmin   = 1;
	ob->xmax   = w->cols - 1;
	ob->ymin   = 1;
	ob->ymax   = w->rows - 1;
	return ob;
}

OBJ* InitCar(WIN* w, int col)
{
	OBJ* ob	   = (OBJ*)malloc(sizeof(OBJ));						// C
	ob->bflag  = 1;
	ob->color  = col;
	ob->win    = w;
	ob->width  = 4;
	ob->height = 2;
	ob->mv     = MVB_FACTOR;

	ob->shape = (char**)malloc(sizeof(char*) * ob->height);				// array of pointers (char*)
	for(int i = 0; i < ob->height; i++)
		ob->shape[i] = (char*)malloc(sizeof(char) * (ob->width + 1));		// +1: end-of-string (C): '\0'

	strcpy(ob->shape[0],"[OO>");
	strcpy(ob->shape[1],"[OO>");

	ob->xmin   = 1;
	ob->xmax   = w->cols - 1;
	ob->ymin   = 1;
	ob->ymax   = w->rows - 1;
	InitPos(ob, 2,19); //ob->xmin, ob->ymin); //testing so far
	return ob;
}

void ShowTimer(WIN* W, float pass_time)
{
	mvwprintw(W->window,1,9,"%.2f",pass_time);
	wrefresh(W->window);
}

//calling show function twice isnt the best but i will fix it latter 
void MoveFrog(OBJ* ob, char ch, unsigned int frame)
{
	if (frame - ob->mv >= MVC_FACTOR)
	{
		switch( ch ) {
			case 'w': Show(ob,0,-1);Show(ob,0,-1);	break;
			case 's': Show(ob,0,1);Show(ob,0,1);	break;
			case 'a': Show(ob,-1,0);Show(ob,-1,0);	break;
			case 'd': Show(ob,1,0);Show(ob,1,0);		break;
			case 'q': Show(ob,-1,-1);Show(ob,-1,-1);	break;
			case 'e': Show(ob,1,-1);Show(ob,1,-1);	break;
			case 'z': Show(ob,-1,1);Show(ob,-1,1);	break;
			case 'c': Show(ob,1,1);Show(ob,1,1);
		}
		ob->mv = frame;
	}
}
int Collision(OBJ* c, OBJ* b)								// collision of two boxes
{
	if ( ((c->y >= b->y && c->y < b->y+b->height) || (b->y >= c->y && b->y < c->y+c->height)) &&
	    ((c->x >= b->x && c->x < b->x+b->width) || (b->x >= c->x && b->x < c->x+c->width)) )
		return 1;
	else 	return 0;
}

void Sleep(unsigned int tui) { usleep(tui * 1000); } 					// micro_sec = frame_time * 1000

TIMER* InitTimer(WIN* status)
{
	TIMER* timer = (TIMER*)malloc(sizeof(TIMER));
	timer->frame_no = 1;
	timer->frame_time = FRAME_TIME;
	timer->pass_time = PASS_TIME / 1.0;
	return timer;
}

int UpdateTimer(TIMER* T, WIN* status)							// return 1: time is over; otherwise: 0
{
	T->frame_no++;
	T->pass_time = PASS_TIME - (T->frame_no * T->frame_time / 1000.0);
	if (T->pass_time < (T->frame_time / 1000.0) ) T->pass_time = 0; 		// make this zero (floating point!)
	else Sleep(T->frame_time);
	ShowTimer(status,T->pass_time);
	if (T->pass_time == 0) return 1;
	return 0;
}

int MainLoop(WIN* status, OBJ* frog, OBJ* car, TIMER* timer) 			// 1: timer is over, 0: quit the game
{
	int ch;
	int pts = 0;
	while ( (ch = wgetch(status->window)) != QUIT )					// NON-BLOCKING! (nodelay=TRUE)
	{
		if (ch == ERR) ch = NOKEY;						// ERR is ncurses predefined
		/* change background or move; update status */
		else
		{
			if (ch == 'b') {  Show(frog,0,0); }
			else MoveFrog(frog, ch, timer->frame_no);
		}
		if(Collision(frog , car))
			mvwaddstr(status->window, 1, 1, "are you stupid?");
		
		flushinp();                     					// clear input buffer (avoiding multiple key pressed)
		/* update timer */
		if (UpdateTimer(timer,status)) return pts;				// sleep inside
	}
	return 0;
}

void rColorChange(WINDOW* win, int row, int color_pair) {
   
    wattron(win, COLOR_PAIR(color_pair));
    int cols = getmaxx(win); // Get the number of columns in the window
    for (int col = 1; col < cols - 1; col++) {    //if widow is from (0 to n) it will color (1 to n-1)
        mvwprintw(win, row, col, " ");
    }

    wattroff(win, COLOR_PAIR(color_pair));
    wrefresh(win);
}

// input -> window playable , lvl to generate, probability of green 
void lvlGen(WIN* W, int lvlChoice, int grProb)
{
	int num = 0; 
	int lastGr = 0;     // 1 if last generated is green 
						// 0 if is street  - generated from the top so lastGr=1
	for(int i = 3; i < W->rows - 3; i+=2)
	{
		num = RA(i,99);
		if(num%grProb!=0)			//if
		{
			rColorChange(W->window, i, CAR_COLOR);
			rColorChange(W->window, i+1, CAR_COLOR);
		}
	}
}

int main()
{
	WINDOW *mainwin = Start();

	Welcome(mainwin);

	WIN* playwin = Init(mainwin, ROWS, COLS, OFFY, OFFX, PLAY_COLOR, BORDER, DELAY_ON);
	WIN* statwin = Init(mainwin, 3, COLS, ROWS+OFFY, OFFX, STAT_COLOR, BORDER, DELAY_OFF);
	int lvlChoice=0;
	lvlChoice = Menu(playwin);
	mvwprintw(statwin->window,1,1, "%d", lvlChoice);
	wrefresh(statwin->window);

	if(lvlChoice == 1) lvlGen(playwin, 1, 3);

	OBJ* frog = InitFrog(playwin,FROG_COLOR);
	OBJ* car = InitCar(playwin,CAR_COLOR);
	Show(frog,0,0);
	Show(car,0,0);

	////
	TIMER* timer = InitTimer(statwin);
	int result;
	if ( (result = MainLoop(statwin, frog, car, timer)) == 0)  EndGame("You have decided to quit the game.",statwin);

	getch();

	delwin(playwin->window);							// Clean up (!)
	delwin(statwin->window);
	delwin(mainwin);
	endwin();
	refresh();
	return 0;
}