#include "MCTSPlayer.hpp"

namespace game
{

MCTSPlayer::MCTSPlayer(unsigned int id, AbstractGame *game)
    : AbstractPlayer(id), game(game)
{
}

AbstractBoardCell *MCTSPlayer::action(AbstractBoard *board)
{
    mcts::MCTSConstraints constraints = {
        .time = 1000 //ms
    };
    mcts::Tree tree(game, this, constraints);
    tree.begin();
    AbstractBoardCell *bestMove = tree.bestMove();
    board->performMove(getId(), bestMove);
    return bestMove;
}
} // namespace game