#include <istream>
#include <iostream>

#include "coins.hpp"
#include "pannorama.hpp"

using namespace std;

int main(int, char**)
{
    
    char ch;
    
    cout << "P : pannoramma" << endl;
    cout << "c : coins" << endl;
    cout << "Votre choise : " ;
    cin >> ch;
    
    if (ch == 'p') {
        pannorama();
    }
    else if (ch == 'c') {
        coins();
    }
    // the camera will be deinitialized automatically in VideoCapture destructor
    return 0;
}