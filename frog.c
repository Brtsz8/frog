//mima

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

#define FRAME_TIME	60
#define PASS_TIME	20
#define MVB_FACTOR	1						// move every FRAME_TIME * MVB_FACTOR [ms] BALL
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
#define ROWS		36							
#define COLS		60
#define OFFY		4
#define OFFX		8

#define MAX_CARS 	30
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


//im not sure if i evere used it??? 
typedef struct{
	int id;
	int lane;			//count from the top
	int pos;			//position on the lane 
}CAR;

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

//function modified so dx,dy can be any whole number
//f.e. for frog dx,dy usually is one of -2,0,2
void Show(OBJ* ob, int dx, int dy)		
{
	char *sw = (char*)malloc(sizeof(char) * ob->width);
	memset(sw,' ',ob->width);

	wattron(ob->win->window,COLOR_PAIR(ob->color));
	

	if ((dy >= 1) && (ob->y + ob->height < ob->ymax))
	{
		ob->y += dy;
		mvwprintw(ob->win->window, ob->y-1, ob->x,sw);
		mvwprintw(ob->win->window, ob->y-2, ob->x,sw);
	}
	if ((dy <=-1) && (ob->y > ob->ymin))
	{
		ob->y += dy;
		mvwprintw(ob->win->window, ob->y+ob->height, ob->x,sw);
		mvwprintw(ob->win->window, ob->y+ob->height + 1, ob->x,sw);
	}

	if ((dx >= 1) && (ob->x < ob->xmax))
	{
		ob->x += dx;
		for(int i = 0; i < ob->height; i++){
			mvwprintw(ob->win->window, ob->y+i, ob->x-1," ");
			if(ob->width==2)mvwprintw(ob->win->window, ob->y+i, ob->x-2," "); 
			//cars move 1 to the right but frog dont, so cars only need one 
			//space painted before them 
			//thing above might be not optimal solution but it works
			//will not work when car movement reversed 
		}
	}
	if ((dx <=-1) && (ob->x > ob->xmin))
	{
		ob->x += dx;
		for(int i = 0; i < ob->height; i++){
			mvwprintw(ob->win->window, ob->y+i, ob->x+ob->width," ");
			mvwprintw(ob->win->window, ob->y+i, ob->x+ob->width+1," ");
		}
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


//TO DO LATER !!!!/////
void EndGame(const char* info, WIN* W)					
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

	OBJ* ob	   = (OBJ*)malloc(sizeof(OBJ));
	ob->bflag  = 1;									
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
	ob->xmax   = w->cols -1;
	ob->ymin   = 1;
	ob->ymax   = w->rows -1;
	return ob;
}

OBJ* InitCar(WIN* w, int col, int spawnCol, int spawnRow)
{
	OBJ* ob	   = (OBJ*)malloc(sizeof(OBJ));						
	ob->bflag  = 1;
	ob->color  = col;
	ob->win    = w;
	ob->width  = 4;
	ob->height = 2;
	ob->mv     = RA(MVB_FACTOR,MVB_FACTOR+2);

	ob->shape = (char**)malloc(sizeof(char*) * ob->height);				// array of pointers (char*)
	for(int i = 0; i < ob->height; i++)
		ob->shape[i] = (char*)malloc(sizeof(char) * (ob->width + 1));		// +1: end-of-string (C): '\0'

	strcpy(ob->shape[0],"[OO]");
	strcpy(ob->shape[1],"[OO]");

	ob->xmin   = 1;
	ob->xmax   = w->cols -1;
	ob->ymin   = 1;
	ob->ymax   = w->rows -1;
	InitPos(ob, spawnCol,spawnRow); //ob->xmin, ob->ymin); //testing so far
	return ob;
}

void ShowTimer(WIN* W, float pass_time)
{
	mvwprintw(W->window,1,9,"%.2f",pass_time);
	wrefresh(W->window);
}

//calling show function twice isnt the best but i will fix it latter
//fixed , tests optional  
void MoveFrog(OBJ* ob, char ch, unsigned int frame,int* isRoad)
{
	if (frame - ob->mv >= MVC_FACTOR)
	{
		switch( ch ) {
			case 'w': Show(ob,0,-2);	break;
			case 's': Show(ob,0,2);		break;
			case 'a':
				if(ob->x==1)
				{								//in case frogs in the left corner it moves frog to
					Show(ob,COLS-4,0);			//right corner and repaints the left corner
					mvwprintw(ob->win->window, ob->y+1, 1,"  ");
					mvwprintw(ob->win->window, ob->y, 1,"  ");
				}	
				else Show(ob,-2,0);	
				break;
			case 'd': 
				if(ob->x+ob->width==COLS-1)		//see coment above, same thing other corner
				{
					Show(ob,4-COLS,0);		
					mvwprintw(ob->win->window, ob->y+1, COLS-3,"  ");        
					mvwprintw(ob->win->window, ob->y, COLS-3,"  ");
				}	 		
				else Show(ob,2,0);
				break;		
		}
		ob->mv = frame;
	}
}


void MoveCar(OBJ* ob, int Cx, int frame)
{
	
	int dx = 0;
	
	if (frame % ob->mv == 0)							// every ob->mv-th frame make a move
	{
		dx = Cx;										
		if(ob->x+ob->width==COLS-1){	
			wattron(ob->win->window, COLOR_PAIR(CAR_COLOR));
			mvwprintw(ob->win->window, ob->y+1, COLS-5,"    ");        
			mvwprintw(ob->win->window, ob->y, COLS-5,"    ");
			wattroff(ob->win->window, COLOR_PAIR(CAR_COLOR));
			Show(ob,ob->width+2-COLS,0);

			//randomly assigns new speed to the car when car reaches the end
			ob->mv= RA(MVB_FACTOR,MVB_FACTOR+2);
		}
		else Show(ob,dx,0);
		
	}
}
int Collision(OBJ* c, OBJ* b)								// collision of two objects
{
	if ( ((c->y >= b->y && c->y < b->y+b->height) || (b->y >= c->y && b->y < c->y+c->height)) &&
	    ((c->x >= b->x && c->x < b->x+b->width) || (b->x >= c->x && b->x < c->x+c->width)) )
		return 1;
	else 	return 0;
}

void Sleep(unsigned int tui) { usleep(tui * 1000); } 		// micro_sec = frame_time * 1000

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
void rColorChange(WINDOW* win, int row, int color_pair) {
   
    wattron(win, COLOR_PAIR(color_pair));
    int cols = getmaxx(win); // Get the number of columns in the window
    for (int col = 1; col < cols - 1; col++) {    //if widow is from (0 to n) it will color (1 to n-1)
        mvwprintw(win, row, col, " ");
    }

    wattroff(win, COLOR_PAIR(color_pair));
    wrefresh(win);
}
// input -> window playable , lvl to generate from randNum, probability of green 
void lvlGen(WIN* W, int* isRoad, int grProb)
{
	int lastGr = 0;     // 1 if last generated is green 
	int num = 0;		// 0 if is street  - generated from the top so lastGr=1

	OBJ* car [MAX_CARS];

	for(int i = 3; i < W->rows - 3; i+=2)
	{
		num = RA(i,99-i);
		if(lastGr){
			*(isRoad + i) = 1;
			rColorChange(W->window, i, CAR_COLOR);
			rColorChange(W->window, i+1, CAR_COLOR);
			lastGr=0;
			continue;
		}
		if(num%grProb!=0)			
		{
			*(isRoad + i) = 1;
			rColorChange(W->window, i, CAR_COLOR);
			rColorChange(W->window, i+1, CAR_COLOR);
		}else{
			*(isRoad + i)= 0;
			lastGr=1;
		}
	}
}

OBJ** carGen(WIN* W, int* isRoad, int numCars)
{
	//array of pointers to OBJ
	OBJ** cars = malloc((numCars+1) * sizeof(OBJ*));

	int toSpawn = numCars;
	int i = 0;
	
	while(toSpawn)
	{
		if(*(isRoad+3+i) == 1)
		{
			//setting up memory for each car object
			cars[numCars-toSpawn]=malloc(sizeof(OBJ));
			OBJ* car= InitCar(W,CAR_COLOR,RA(1,(COLS-4)/2),i+3);
			//cars index go from 0 to carNum
			cars[numCars-toSpawn] = car; 
			Show(cars[numCars-toSpawn],0,0);
			toSpawn--; 
		}
		i++;
	}

	return cars;
}


void lvlRepaint(WIN* W, int* isRoad)
{
	for(int i = 1; i < W->rows - 2; i+=2)
	{
		if(*(isRoad+i) == 1)			//if
		{
			rColorChange(W->window, i, CAR_COLOR);
			rColorChange(W->window, i+1, CAR_COLOR);
			//Show(InitCar(W,CAR_COLOR,2+i,i),0,0);
		}
		else{
			rColorChange(W->window, i, PLAY_COLOR);
			rColorChange(W->window, i+1, PLAY_COLOR);
		}
	}
}

int MainLoop(WIN* status, WIN* W,int* isRoad, OBJ* frog, OBJ** cars, TIMER* timer) 			// 1: timer is over, 0: quit the game
{
	int ch;
	int pts = 0;

	int roadCount = 0;
	for(int i =0; i<ROWS; i++)
		if(isRoad[i]==1) roadCount++;
	
	while ( (ch = wgetch(status->window)) != QUIT )					// NON-BLOCKING! (nodelay=TRUE)
	{
		if (ch == ERR) ch = NOKEY;						// ERR is ncurses predefined
		/* change background or move; update status */
		else
		{	
			if (ch == 'b') {  Show(frog,0,0); }
			else {
				MoveFrog(frog, ch, timer->frame_no, isRoad);
				Show(frog,0,0);	
			}
		}
		
		//moves the car and checks for collison 
		//for now it returns to the main screen 
		//but some sort of end screen shoul be added 
		for(int i =0; i<roadCount; i++){
			MoveCar(cars[i],1,timer->frame_no);
			if(Collision(frog , cars[i]))
				return 0;
		}
			
	
		flushinp();                     					// clear input buffer (avoiding multiple key pressed)
		/* update timer */
		if (UpdateTimer(timer,status)) return pts;				// sleep inside
		

	}
	return 0;
}


int main()
{
	WINDOW *mainwin = Start();

	Welcome(mainwin);

	WIN* playwin = Init(mainwin, ROWS, COLS, OFFY, OFFX, PLAY_COLOR, BORDER, DELAY_ON);
	WIN* statwin = Init(mainwin, 5, COLS, ROWS+OFFY, OFFX, STAT_COLOR, BORDER, DELAY_OFF);
	
	int lvlChoice=0;
	while(1){
		lvlChoice = Menu(playwin);
		wrefresh(statwin->window);

		//1 -> road , 0-> grass
		int isRoad[ROWS];
		//for lvl 1 -> seed 3, for 2 -> 5 ect
		lvlGen(playwin,isRoad, 2*lvlChoice+1);

		//returns array of objects - cars , and shows them initially 
		int roadCount = 0;
		for(int i =0; i<ROWS; i++)
			if(isRoad[i]==1) roadCount++;
		int numCars = roadCount;
		OBJ** cars = carGen(playwin,isRoad,numCars);

		OBJ* frog = InitFrog(playwin,FROG_COLOR);
		Show(frog,0,0);

		////
		TIMER* timer = InitTimer(statwin);
		int result;
		if ( (result = MainLoop(statwin,playwin, isRoad,frog, cars, timer)) == 0)  EndGame("You have decided to quit the game.",statwin);

		getch();
	}

	delwin(playwin->window);							// Clean up (!)
	delwin(statwin->window);
	delwin(mainwin);
	endwin();
	refresh();
	return 0;
}