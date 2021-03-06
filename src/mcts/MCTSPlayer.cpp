#ifndef NUMBER_THREADS
#define NUMBER_THREADS 1
#endif

#if NUMBER_THREADS > 1
#include <thread>
#endif

#include <functional>
#include <algorithm>
#include <assert.h>

#include "../game_logic/AbstractBoard.hpp"
#include "Tree.hpp"
#include "MCTS.hpp"

#include "../game_logic/tic_tac_toe/BoardCell.hpp"
#include "../game_logic/tic_tac_toe/Player.hpp"

#include "../game_logic/penguin/BoardCell.hpp"
#include "../game_logic/penguin/HumanPlayer.hpp"
#include "../game_logic/penguin/PenguinPawn.hpp"

#include "MCTSPlayer.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, dispatchEvent, (), {
    console.log("Dispatching event from wasm");
    setTimeout(
        function() {
            let event = new CustomEvent('new_best_move');
            document.dispatchEvent(event);
        },
        0);
});
#endif

namespace mcts
{

    template <class CellT, class PlayerT, class PawnT>
    MCTSPlayer<CellT, PlayerT, PawnT>::MCTSPlayer(
        game::AbstractGame<CellT, PlayerT, PawnT> *const &game,
        const mcts::MCTSConstraints &constraints)
        : game(game),
          constraints(constraints)
    {
    }

    template <class CellT, class PlayerT, class PawnT>
    MCTSPlayer<CellT, PlayerT, PawnT>::~MCTSPlayer()
    {
        for (mcts::Tree<CellT, PlayerT, PawnT> *tree : trees)
            delete tree;
        trees.clear();
    }

    template <class CellT, class PlayerT, class PawnT>
    game::Move<CellT, PawnT> MCTSPlayer<CellT, PlayerT, PawnT>::bestMove()
    {
        //begins the mcts search
        unleash_mcts();

        //merge every result into 1 tree
        mcts::Tree<CellT, PlayerT, PawnT> tree = joinTrees();

        const game::Move<CellT, PawnT> move = tree.bestMove();

        auto ret = getCorrespondingMove(move);

        latest_result = {ret, tree.getRootNode().score, tree.getRootNode().visits};

#ifdef __EMSCRIPTEN__
        dispatchEvent();
#endif

        return ret;
    }

    template <class CellT, class PlayerT, class PawnT>
    void MCTSPlayer<CellT, PlayerT, PawnT>::updateTree(const game::Move<CellT, PawnT> &last_move_played)
    {
        //Moves all rootNodes to the corresponding cell
        for (size_t i = 0; i < trees.size(); i++)
        {
            trees.at(i)->moveRootToMove(last_move_played);
        }
    }

    template <class CellT, class PlayerT, class PawnT>
    void MCTSPlayer<CellT, PlayerT, PawnT>::unleash_mcts()
    {
        size_t i;
        if (trees.size() == 0)
        {
            trees.reserve(NUMBER_THREADS);
            for (i = 0; i < NUMBER_THREADS; i++)
                trees.push_back(new mcts::Tree<CellT, PlayerT, PawnT>(game->clone(), constraints));
        }
        //We test here if there are more than 1 thread
        //We don't really need to launch a new thread if we only have 1 tree
#if NUMBER_THREADS > 1
        // std::vector<std::thread> threads;
        pthread_t threads[NUMBER_THREADS];
        MCTS<CellT, PlayerT, PawnT> *ais[NUMBER_THREADS];
        pthread_attr_t attr;
        void *status;

        // Initialize and set thread joinable
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        int rc;

        //Create an ai for every thread with the clone of the board
        //If we used the current board, it gets very messy very quick
        for (i = 0; i < NUMBER_THREADS; i++)
        {
            // mcts::MCTS<CellT, PlayerT, PawnT> ai(trees.at(i), constraints);
            ais[i] = new MCTS<CellT, PlayerT, PawnT>(trees.at(i), constraints);
            rc = pthread_create(&threads[i], &attr, &MCTS<CellT, PlayerT, PawnT>::begin_mcts, static_cast<void *>(ais[i]));
            if (rc)
            {
                std::cout << "Error:unable to create thread," << rc << std::endl;
                assert(false && "Error:unable to create thread");
                exit(-1);
            }
            // threads.push_back(std::thread(&mcts::MCTS<CellT, PlayerT, PawnT>::begin, ai));
        }

        // free attribute and wait for the other threads
        pthread_attr_destroy(&attr);
        bool all_finished = false;
        while (!all_finished)
        {
            all_finished = true;
            for (i = 0; i < NUMBER_THREADS; i++)
            {
                if (ais[i])
                {
                    if (all_finished)
                        all_finished = false;
                    // threads.at(i).join();
                    rc = pthread_tryjoin_np(threads[i], &status);
                    if (rc)
                    {
                        // std::cout << "Error:unable to join," << rc << std::endl;
                        // assert(false && "Error:unable to join thread");
                        // exit(-1);
                    }
                    else
                    {
                        // thread joined, cleaning
                        if (ais[i])
                            delete ais[i];
                        ais[i] = nullptr;
                    }
                }
            }
#ifdef __EMSCRIPTEN__
            emscripten_sleep(10);
#endif
        }
        // pthread_exit(NULL);

#else
        //Create only 1 ai and run it
        mcts::MCTS<CellT, PlayerT, PawnT> ai(trees.at(0), constraints);
        // TODO check for the life time of the object, where is it destroyed ?
        ai.begin();
#endif
    }

    template <class CellT, class PlayerT, class PawnT>
    const game::Move<CellT, PawnT> MCTSPlayer<CellT, PlayerT, PawnT>::getCorrespondingMove(const game::Move<CellT, PawnT> &move) const
    {
        std::vector<game::Move<CellT, PawnT>> moves = game->getAvailableMoves(game->board->getPlayerById(move.pawn->getOwner()->getId()));

        assert("The next player that is set to play is not the one that does the move" && game->getPlayerToPlay() == move.pawn->getOwner()->getId());

        const auto it = std::find_if(
            std::begin(moves),
            std::end(moves),
            [move](const game::Move<CellT, PawnT> &move_testing) -> bool { return move == move_testing; });

        assert("The move doesn't translate to the game outside the tree(s)/MCTS context" && it != std::end(moves));

        if (it != std::end(moves))
        {
            return *it;
        }

        return {nullptr, nullptr, nullptr};
    }

    template <class CellT, class PlayerT, class PawnT>
    mcts::Tree<CellT, PlayerT, PawnT> MCTSPlayer<CellT, PlayerT, PawnT>::joinTrees()
    {
        mcts::Tree<CellT, PlayerT, PawnT> tree(game, constraints);
        //For every tree, merge it into the new tree
        for (int i = 0; i < NUMBER_THREADS; i++)
        {
            tree.merge(trees.at(i));
        }
        return tree;
    }
    template <class CellT, class PlayerT, class PawnT>
    const Result<CellT, PawnT> MCTSPlayer<CellT, PlayerT, PawnT>::getResult() const
    {
        return latest_result;
    }

    template class MCTSPlayer<game::tic_tac_toe::BoardCell, game::tic_tac_toe::Player, game::tic_tac_toe::Player>;
    template class MCTSPlayer<game::penguin::BoardCell, game::penguin::HumanPlayer, game::penguin::PenguinPawn>;

} // namespace mcts