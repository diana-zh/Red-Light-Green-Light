// SYDE 121 FINAL PROJECT
// TEAM 01: HET PATEL, SOHAIL SAYED, DIANA ZHANG
// GAME: RED LIGHT GREEN LIGHT

// ------------------------------------------------------------------------------------------------------------------

// HOW TO PLAY

// --> Move using W (UP), S (DOWN), A (LEFT), D (RIGHT)
// --> Start off with 3 lives
// --> You can only move when light is green
// --> If you move when light is red, you lose a life
// --> If you lose all three lives, you get a loss and the game is restarted
// --> IF you reach the green victory line, you win
// --> When you win, you gain 3 more lives and the game is retstarted
// --> HAVE FUN!!

// ------------------------------------------------------------------------------------------------------------------

// COMPILING & TECHNICAL INSTRUCTIONS

// compile with: clang++ -std=c++20 -Wall -Werror -Wextra -Wpedantic -g3 -o team01-RedLightGreenLightGame team01-RedLightGreenLightGame.cpp
// run with: ./team01-RedLightGreenLightGame 2> /dev/null
// run with: ./team01-RedLightGreenLightGame 2> debugoutput.txt
//  "2>" redirect standard error (STDERR; cerr)
//  /dev/null is a "virtual file" which discard contents

// Works best in Visual Studio Code if you set:
//   Settings -> Features -> Terminal -> Local Echo Latency Threshold = -1

// ------------------------------------------------------------------------------------------------------------------

// https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono> // for dealing with time intervals
#include <cmath> // for max() and min()
#include <termios.h> // to control terminal modes
#include <unistd.h> // for read()
#include <fcntl.h> // to enable / disable non-blocking read()

// Because we are only using #includes from the standard, names shouldn't conflict
using namespace std;

// ------------------------------------------------------------------------------------------------------------------

// TYPES

// Using signed and not unsigned to avoid having to check for ( 0 - 1 ) being very large
struct position { int row; int col; };
typedef vector< string > stringvector;

// This will be the moving player
// Although we will only have 1 player in this game, using a struct helps keep all the player properties linked to one variable
// Thus, easier to understand code and what the individual members are refering to
struct player 
{
    struct position position {17, 3};       // Starting position
    int lives = 3;                          // Lives remaining
    struct position prev_position {17, 3};  // Previous position
    int wins = 0;                           // Number of wins
    int losses = 0;                         // Number of losses
};

// ------------------------------------------------------------------------------------------------------------------

// CONSTANTS

// Disable JUST this warning (in case students choose not to use some of these constants)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

const char NULL_CHAR     { 'z' };
const char UP_CHAR       { 'w' };
const char DOWN_CHAR     { 's' };
const char LEFT_CHAR     { 'a' };
const char RIGHT_CHAR    { 'd' };
const char QUIT_CHAR     { 'q' };
const char CREATE_CHAR   { 'c' };

const string ANSI_START { "\033[" };
const string START_COLOUR_PREFIX {"1;"};
const string START_COLOUR_SUFFIX {"m"};
const string STOP_COLOUR  {"\033[0m"};

const unsigned int COLOUR_IGNORE  { 0 }; // this is a little dangerous but should work out OK
const unsigned int COLOUR_GREEN   { 92 };
const unsigned int COLOUR_RED     { 31 };
const unsigned int COLOUR_WHITE   { 37 };

const unsigned int VICTORY_LINE_COL { 95 }; // column for victory line
const position LIGHT_POSITION {2, 80}; // Position of light 

// Could have used indirection for ANSI text colour but these are constants 
// and indrection makes the strings very long so it's difficult to actually see
// what the person figure and light actually look like
const stringvector PERSON_FIGURE
    {
    {"\033[1;36m   \\0_ \033[0m"},
    {"\033[1;36m   /    \033[0m"},
    {"\033[1;36m _/\\   \033[0m"},
    {"\033[1;36m   /    \033[0m"}
    };

const stringvector RedLight
    {
    {"\033[1;31m  ■■■■  \033[0m"},
    {"\033[1;31m ■■■■■■ \033[0m"},
    {"\033[1;31m■■■■■■■■\033[0m"},
    {"\033[1;31m■■■■■■■■\033[0m"},
    {"\033[1;31m ■■■■■■ \033[0m"},
    {"\033[1;31m  ■■■■  \033[0m"},
    };

const stringvector GreenLight
    {
    {"\033[1;92m  ■■■■  \033[0m"},
    {"\033[1;92m ■■■■■■ \033[0m"},
    {"\033[1;92m■■■■■■■■\033[0m"},
    {"\033[1;92m■■■■■■■■\033[0m"},
    {"\033[1;92m ■■■■■■ \033[0m"},
    {"\033[1;92m  ■■■■  \033[0m"},
    };


#pragma clang diagnostic pop

// ------------------------------------------------------------------------------------------------------------------

// Globals

struct termios initialTerm;

// ------------------------------------------------------------------------------------------------------------------

// FUNCTIONS REUSED FROM FISGES CODE

// Utilty Functions

// These two functions are taken from StackExchange and are 
// all of the "magic" in this code.
auto SetupScreenAndInput() -> void
{
    struct termios newTerm;
    // Load the current terminal attributes for STDIN and store them in a global
    tcgetattr(fileno(stdin), &initialTerm);
    newTerm = initialTerm;
    // Mask out terminal echo and enable "noncanonical mode"
    // " ... input is available immediately (without the user having to type 
    // a line-delimiter character), no input processing is performed ..."
    newTerm.c_lflag &= ~ICANON;
    newTerm.c_lflag &= ~ECHO;
    newTerm.c_cc[VMIN] = 1;
 
    // Set the terminal attributes for STDIN immediately
    auto result { tcsetattr(fileno(stdin), TCSANOW, &newTerm) };
    if ( result < 0 ) { cerr << "Error setting terminal attributes [" << result << "]" << endl; }
}
auto TeardownScreenAndInput() -> void
{
    // Reset STDIO to its original settings
    tcsetattr( fileno( stdin ), TCSANOW, &initialTerm );
}
auto SetNonblockingReadState( bool desiredState = true ) -> void
{
    auto currentFlags { fcntl( 0, F_GETFL ) };
    if ( desiredState ) { fcntl( 0, F_SETFL, ( currentFlags | O_NONBLOCK ) ); }
    else { fcntl( 0, F_SETFL, ( currentFlags & ( ~O_NONBLOCK ) ) ); }
    cerr << "SetNonblockingReadState [" << desiredState << "]" << endl;
}

// Everything from here on is based on ANSI codes
// Note the use of "flush" after every write to ensure the screen updates
auto ClearScreen() -> void { cout << ANSI_START << "2J" << flush; }
auto MoveTo( unsigned int x, unsigned int y ) -> void { cout << ANSI_START << x << ";" << y << "H" << flush; }
auto HideCursor() -> void { cout << ANSI_START << "?25l" << flush; }
auto ShowCursor() -> void { cout << ANSI_START << "?25h" << flush; }
auto GetTerminalSize() -> position
{
    // This feels sketchy but is actually about the only way to make this work
    MoveTo(999,999);
    cout << ANSI_START << "6n" << flush ;
    string responseString;
    char currentChar { static_cast<char>( getchar() ) };
    while ( currentChar != 'R')
    {
        responseString += currentChar;
        currentChar = getchar();
    }
    // format is ESC[nnn;mmm ... so remove the first 2 characters + split on ; + convert to unsigned int
    // cerr << responseString << endl;
    responseString.erase(0,2);
    // cerr << responseString << endl;
    auto semicolonLocation = responseString.find(";");
    // cerr << "[" << semicolonLocation << "]" << endl;
    auto rowsString { responseString.substr( 0, semicolonLocation ) };
    auto colsString { responseString.substr( ( semicolonLocation + 1 ), responseString.size() ) };
    // cerr << "[" << rowsString << "][" << colsString << "]" << endl;
    auto rows = stoul( rowsString );
    auto cols = stoul( colsString );
    position returnSize { static_cast<int>(rows), static_cast<int>(cols) };
    // cerr << "[" << returnSize.row << "," << returnSize.col << "]" << endl;
    return returnSize;
}

auto MakeColour( string inputString, 
                 const unsigned int foregroundColour = COLOUR_WHITE,
                 const unsigned int backgroundColour = COLOUR_IGNORE ) -> string
{
    string outputString;
    outputString += ANSI_START;
    outputString += START_COLOUR_PREFIX;
    outputString += to_string( foregroundColour );
    if ( backgroundColour ) 
    { 
        outputString += ";";
        outputString += to_string( ( backgroundColour + 10 ) ); // Tacky but works
    }
    outputString += START_COLOUR_SUFFIX;
    outputString += inputString;
    outputString += STOP_COLOUR;
    return outputString;
}

// ------------------------------------------------------------------------------------------------------------------
// FUNCTIONS CREATED BY TEAM (SOME MANIPULTED FROM FISHES CODE)

// PLAYER LOGIC
// Used pass by reference since we want to manipulate the player's position
// Made currentChar a constant since we are not changing it
auto UpdatePlayerPosition( player & user, const char currentChar ) -> void
{
    // Update previous position values
    user.prev_position.row = user.position.row;
    user.prev_position.col = user.position.col;

    // Update current position values
    if ( currentChar == UP_CHAR )    { user.position.row -= 1; }
    if ( currentChar == DOWN_CHAR )  { user.position.row += 1; }
    if ( currentChar == LEFT_CHAR )  { user.position.col -= 1; }
    if ( currentChar == RIGHT_CHAR ) { user.position.col += 1; }

    // Ensure player within boundries of game
    user.position.row = max( 10, min( 25, user.position.row ) );
    user.position.col = max(  3, min( 95, user.position.col ) );

}

// Function to draw person figure and light every refresh
// From sample code
auto DrawSprite( position targetPosition, stringvector sprite,
                 const unsigned int foregroundColour = COLOUR_WHITE,
                 const unsigned int backgroundColour = COLOUR_IGNORE)
{
    MoveTo( targetPosition.row, targetPosition.col );
    for ( auto currentSpriteRow { 0 } ;
                currentSpriteRow < static_cast<int>(sprite.size()) ;
                currentSpriteRow++ )
    {
        cout << MakeColour( sprite[currentSpriteRow], foregroundColour, backgroundColour );
        MoveTo( ( targetPosition.row + ( currentSpriteRow + 1 ) ) , targetPosition.col );
    };
}

// GAME BORDER/BOUNDRIES FUNCTION
// All constants since we are only reading the values not changing
auto DrawBorder (const unsigned int width, const unsigned int height, const char borderChar, const char emptyBorder) -> void
{   
    const string topAndBottomRows (width, borderChar);    // String of width number of astricks for top and bottons rows
    cout << topAndBottomRows << endl;                     // Outputs top border

    // Loop for side borders
    // Used range for loop since we need to repeat for x number of times (no parsing)
    for (unsigned int row { 0 }; row < height - 2; row++)
    {
        string middleRows (width, emptyBorder);     // Creates empty row of spaces of needed width
        middleRows[0] = borderChar;                 // Changes first character of row to astrick 
        middleRows [width - 1] = borderChar;        // Changes last character of row to astrick
        cout << middleRows << endl;                 // Outputs current middle row
    }
    cout << topAndBottomRows << endl;               // Outputs bottom border
}

// Function for green victory line
// All constants since we are only reading the values not changing
auto DrawVictoryLine (const position pos, const unsigned int height) -> void
{      
    // For loop for each row
    // Used range for loop since we need to repeat for x number of times (no parsing)
    for (unsigned int row { 0 }; row < height; row++ ) {
        MoveTo(pos.row + row, pos.col);             // Moves to approprioate position
        
        // Outputs green astrick
        cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << \
                "*" << STOP_COLOUR << flush;
    }
}

// Function to restart game 
// Used pass by reference since we are manipulating player members
auto RestartGame (player & user, const unsigned int new_lives) -> void
{
    user.position = {17, 3};    // Resets user position to front
    user.lives = new_lives;     // Resets number of lives depending on if user won or lost
}

// Function to check if user moved on red light
auto MovedOnRedCheck (player & user, const stringvector CurrentLight) -> void
{
    // If Else Statements To Check If User Moved on Red Light
    if (CurrentLight == RedLight) {

        // Checks if user position has changed from their previous position
        if (user.prev_position.row != user.position.row or user.prev_position.col != user.position.col) {
            
            user.lives -= 1; // If so, decrease lives by 1

            // Outputs message that user moved on red light and shows lives remaining
            MoveTo(15, 45); 
            cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_RED << START_COLOUR_SUFFIX << \
                    "YOU MOVED ON RED LIGHT! YOU NOW HAVE " << user.lives << " LIVES REMAINING." \
                    << STOP_COLOUR << flush;
            
            // Checks if user has no more lives left
            if (user.lives <= 0) {
                ClearScreen();

                // Ouptuts message that user has lost all lives
                MoveTo(15, 20);
                cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_RED << START_COLOUR_SUFFIX << \
                        "YOU LOST! YOU HAVE ZERO LIVES REMAINING. GAME WILL RESTART. IF YOU WANT TO QUIT, PRESS Q." \
                        << STOP_COLOUR << flush;
                
                // Resets game and increases user losses by 1
                RestartGame(user, 3);
                user.losses += 1;
            }

            // Pauses program for user to see message and clears standard in
            // We must clear stdin in case the user pressed any key during message display (we don't want to count those presses when the game resumes)
            sleep(2);
            cin.clear();
            fflush(stdin);
        }
    }
}

// Function to change light colours
// Strategy for changing light
//      Set a duration for the light to remain on
//      Check if difference between start and end time stamp (time elapsed) is more than set duration
//      If so, change the light colour
//      Set new duration
auto ChangeLightColour (stringvector & CurrentLight, int & lightDuration, 
                        const std::chrono::_V2::system_clock::time_point lightStart) -> void
{
    // Calculates time elasped for light
    auto lightEnd { chrono::high_resolution_clock::now() };
    auto lightElapsed { chrono::duration<double, std::milli>(lightEnd - lightStart).count() };

    // If time elapsed is greater than duration, we must change light colour
    if (lightElapsed >= lightDuration) {

        // If current light is red, change to green and create new random time duration
        if (CurrentLight == RedLight) {
            CurrentLight = GreenLight;
            lightDuration += rand() % 1000 + 2000; // Green light stays on longer than red (1s to 3s)
        }

        // Else, change to red and create new random time duration
        else {
            CurrentLight = RedLight;
            lightDuration += rand() % 1000 + 1000; // Red light stays on less than green (1s to 2s)
        }
    }
}

// Function to check if user has reached victory line and update game accordingly if so
auto VictoryLineReachedCheck(player & user) -> void 
{
    if (user.position.col == VICTORY_LINE_COL) {

        // Ouputs message that user has won the game
        MoveTo(15, 20);
        cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << \
                "YOU WON! GAME WILL RESTART WITH 3 LIVES REGAINED! IF YOU WANT TO QUIT, PRESS Q." \
                << STOP_COLOUR << flush;

        // Restarts game and increments user's wins
        RestartGame(user, user.lives+3);
        user.wins += 1;

        // Pauses program for user to see message and clears standard in
        sleep(2);
        cin.clear();
        fflush(stdin);
    }
}

// MAIN GAME FUNCTION
auto main() -> int
{
    // Set Up the system to receive input
    SetupScreenAndInput();

    // Check that the terminal size is large enough for the game
    const position TERMINAL_SIZE { GetTerminalSize() };
    if ( ( TERMINAL_SIZE.row < 30 ) or ( TERMINAL_SIZE.col < 100 ) )
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl <<  "Terminal window must be at least 30 by 100 to run this game" << endl;
        return EXIT_FAILURE;
    }

    HideCursor();

    // State Variables
    player user; 
    unsigned int ticks {0};

    char currentChar { NULL_CHAR }; // the first character will be null since user did not input anything
    string currentCommand; // Declares string to hold commands 

    bool allowBackgroundProcessing { true };

    auto startTimestamp { chrono::steady_clock::now() };
    auto endTimestamp { startTimestamp };
    unsigned int elapsedTimePerTick { 100 }; // Every 0.1s check on things
    
    SetNonblockingReadState( allowBackgroundProcessing );

    // GAME STARTING SCREEN
    ClearScreen();

    // Prints title
    MoveTo(14, 30);
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << \
            "WELCOME TO RED LIGHT GREEN LIGHT" \
            << STOP_COLOUR << flush;

    // Prints credits
    MoveTo(16, 20);
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_RED << START_COLOUR_SUFFIX << \
            "PROGRAMMED BY: HET PATEL, SOHAIL SAYED, DIANA ZHANG" \
            << STOP_COLOUR << flush;

    sleep(2); // Pauses script for 2s so user can read text

    // Strategy for changing light
    //      Set a duration for the light to remain on
    //      Check if difference between start and end time stamp (time elapsed) is more than set duration
    //      If so, change the light colour

    auto lightStart { chrono::high_resolution_clock::now() };   // Start time stamp for light
    auto lightDuration { rand() % 1000 + 2000 };                // Sets light duration between 1s and 3s
    // "Easter Egg" / Issue: light will stay green for a minimum of 1s so the user can play knowing to only move for a 1s regardless if the light stays on for longer
    // We can make the game more challenging by doing rand() % 0 + 1000

    stringvector CurrentLight { GreenLight };                   // Light will begin with green

    // GAME WHILE LOOP
    while( currentChar != QUIT_CHAR )
    {
        endTimestamp = chrono::steady_clock::now();
        auto elapsed { chrono::duration_cast<chrono::milliseconds>( endTimestamp - startTimestamp ).count() };
        
        // We want to process input and update the world when enough time has elapsed
        if ( elapsed >= elapsedTimePerTick )
        {
            // Update ticks and sends error status
            ticks++;
            cerr << "Ticks [" << ticks << "] allowBackgroundProcessing ["<< allowBackgroundProcessing << "] elapsed [" << elapsed << "] currentChar [" << currentChar << "] currentCommand [" << currentCommand << "]" << endl;

            // Updates user position based on current character
            UpdatePlayerPosition( user, currentChar );

            ClearScreen();
            
            // Changes light colour depending on light duration
            ChangeLightColour(CurrentLight, lightDuration, lightStart);

            // Checks if player moved on red light and updates game accordingly
            MovedOnRedCheck(user, CurrentLight);

            // Checks if user has reached victory line
            VictoryLineReachedCheck(user);

            // Draws header and game box
            MoveTo(1, 1);
            DrawBorder (100, 8, '*', ' ');      // draws header
            DrawBorder (100, 21, '*', ' ');     // draws playing field
            
            // Draws victory lines
            DrawVictoryLine ({10, 99}, 19);
            DrawVictoryLine ({10, 100}, 19);

            // Draws user and current light at their respective positions
            DrawSprite( user.position, PERSON_FIGURE );
            DrawSprite( LIGHT_POSITION, CurrentLight );

            // Oututs game title
            MoveTo(3, 25);
            cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << \
                    "RED LIGHT GREEN LIGHT GAME" \
                    << STOP_COLOUR << flush;

            // Outputs game stats
            MoveTo(5, 10);
            cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_RED << START_COLOUR_SUFFIX << \
                    "LIVES REMAINING: " << user.lives << "\t\tWINS: "<< user.wins << "\t\tLOSSES: " << user.losses \
                    << STOP_COLOUR << flush;

            // Clear inputs in preparation for the next iteration
            startTimestamp = endTimestamp;    
            currentChar = NULL_CHAR;
            currentCommand.clear();
        }
        // gets next character
        read( 0, &currentChar, 1 );
    }

    // GAME ENDING SCREEN
    ClearScreen();

    // Prints thank you message
    MoveTo(14, 24);
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_GREEN << START_COLOUR_SUFFIX << \
            "THANK YOU FOR PLAYING RED LIGHT GREEN LIGHT" \
            << STOP_COLOUR << flush;

    // Prints credits
    MoveTo(16, 20);
    cout << ANSI_START << START_COLOUR_PREFIX << COLOUR_RED << START_COLOUR_SUFFIX << \
            "PROGRAMMED BY: HET PATEL, SOHAIL SAYED, DIANA ZHANG" \
            << STOP_COLOUR << flush;
    
    MoveTo(30, 20);

    // Tidy Up and Close Down
    ShowCursor();
    SetNonblockingReadState( false );
    TeardownScreenAndInput();
    cout << endl; // be nice to the next command
    return EXIT_SUCCESS;
    }

// SUGGESTIONS FOR NEXT ITERATION:
//      1. Create different difficulties which user can choose from (ex. Easy, Mediam, Hard)
//              --> Can create different difficulties by changing how long green light stays on for
//              --> Ex. On easy, green light stays on for 1s to 3s. On hard, green light stays on for 0s to 1s
//
//      2. Add randomly generated obstables which the user has to avoid while trying to reach victory line
//              --> If user hits obstables, they lose 1 life and game resumed
//              --> When game restarted, new set of randomly geenrated obstables formed
//
//      3. Add a timer which counts down to 0
//              --> User has to reach victory line before timer hits 0
//              --> If timer hits 0, user loses 1 life and 10s added to clock
//
//      4. Increase difficult after every win when game restarts
//
//      5. Allow user to choose a custom character from premade PERSON FIGURES
