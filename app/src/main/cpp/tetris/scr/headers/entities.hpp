#ifndef ENTITIES_H
#define ENTITIES_H

#include <map>
#include <string>
#include <vector>

using Matrix = std::vector<std::vector<int>>;

extern const std::map<char, Matrix> tetro;
extern const std::map<char, std::string> tiles_path;
extern const std::vector<char> colors;

char get_piece_color(char type);

class Tetro {
private:
    Matrix shape;
    int rotation;

public:
    int x;
    int y;
    int type;

    void setup(int play_colmn);
    void setup_piece(char piece_type, int play_colmn);
    Matrix get_shape() const;
    void rotate(int steps);
    int get_rotation();
};

#endif // ENTITIES_H