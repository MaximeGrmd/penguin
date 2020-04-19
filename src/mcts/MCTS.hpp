#ifndef MCTS_HPP_
#define MCTS_HPP_

#include "../game_logic/AbstractGame.hpp"
#include "Tree.hpp"

namespace mcts
{
template <class CellT, class PlayerT, class PawnT>
class MCTS
{
public:
    explicit MCTS(game::AbstractGame<CellT, PlayerT, PawnT> *game,
         Tree<CellT, PlayerT, PawnT>* tree,
         const MCTSConstraints &constraints);
    ~MCTS();
    void begin();

protected:

    void backPropagateAndRevertAction(int winnerId, Node<CellT, PawnT> *terminalNode);

    game::AbstractBoardCell *getRandomAvailableCellFromBoard() const;

    double formula(
        const Node<CellT, PawnT> &node,
        const Node<CellT, PawnT> &nodeSuccessor) const;

    void doActionOnBoard(const Node<CellT, PawnT> &nodeToGetTheActionFrom);

    void expandNode();

    Node<CellT, PawnT> *selectBestChildAndDoAction(Node<CellT, PawnT> *node);

    Node<CellT, PawnT> *randomChooseChildOrFallbackOnNode(Node<CellT, PawnT> *node) const;

    int randomSimulation() const;

    void expandNode(Node<CellT, PawnT> *nodeToExpand);

private:
    MCTSConstraints constraints;
    game::AbstractGame<CellT, PlayerT, PawnT>* game;
    Tree<CellT, PlayerT, PawnT>* tree;
    Node<CellT, PawnT>* root;
};

}
#endif