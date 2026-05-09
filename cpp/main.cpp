#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// This file intentionally keeps the whole game in one place.
// The project scaffold from CLion is tiny, and a single source file
// makes it easier to understand the entire game flow without jumping
// across a lot of C++ plumbing.

namespace {

// -----------------------------------------------------------------------------
// Card model
// -----------------------------------------------------------------------------
// A standard playing card has a suit and a rank.
// Rank is stored as an integer in the range 2..14, where 11-14 represent
// Jack, Queen, King, and Ace respectively.
enum class Suit {
    Clubs,
    Diamonds,
    Hearts,
    Spades,
};

struct Card {
    Suit suit{};
    int rank{};
};

// Convert a suit to a readable one-letter label.
std::string suitToString(Suit suit) {
    switch (suit) {
        case Suit::Clubs:
            return "C";
        case Suit::Diamonds:
            return "D";
        case Suit::Hearts:
            return "H";
        case Suit::Spades:
            return "S";
    }
    return "?";
}

// Convert the rank value into a human-readable name.
std::string rankToString(int rank) {
    switch (rank) {
        case 11:
            return "Jack";
        case 12:
            return "Queen";
        case 13:
            return "King";
        case 14:
            return "Ace";
        default:
            return std::to_string(rank);
    }
}

// Donation value is the same as the rank value in the ruleset.
int cardValue(const Card& card) {
    return card.rank;
}

// A compact card label that is easy to read in a terminal.
std::string cardToString(const Card& card) {
    return rankToString(card.rank) + suitToString(card.suit);
}

// -----------------------------------------------------------------------------
// Player model
// -----------------------------------------------------------------------------
struct Player {
    std::string name;
    std::vector<Card> hand;
    int totalScore = 0;
    std::vector<int> reignScores;
};

// -----------------------------------------------------------------------------
// Game configuration
// -----------------------------------------------------------------------------
struct GameConfig {
    int cycles = 1;
    int reignTurnsPerSubject = 3;
};

// -----------------------------------------------------------------------------
// Whole-game state
// -----------------------------------------------------------------------------
struct Game {
    GameConfig config;
    std::vector<Player> players;
    std::vector<Card> drawDeck;
    std::vector<Card> discardPile;
    std::vector<Card> favorPile;
    std::mt19937 rng;
};

// -----------------------------------------------------------------------------
// Small input helpers
// -----------------------------------------------------------------------------
// We use getline for everything so the input model stays predictable.
// Mixing formatted extraction (operator>>) with getline tends to create
// annoying newline bugs, especially in interactive console programs.
std::string readLine() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

int promptInt(const std::string& prompt, int minValue, int maxValue, int defaultValue) {
    while (true) {
        std::cout << prompt;
        std::string line = readLine();

        if (line.empty()) {
            return defaultValue;
        }

        std::stringstream ss(line);
        int value = 0;
        char extra = '\0';
        if (ss >> value && !(ss >> extra) && value >= minValue && value <= maxValue) {
            return value;
        }

        std::cout << "Please enter a whole number from " << minValue << " to " << maxValue
                  << ", or press Enter for the default.\n";
    }
}

bool promptYesNo(const std::string& prompt, bool defaultValue = false) {
    while (true) {
        std::cout << prompt;
        std::string line = readLine();

        if (line.empty()) {
            return defaultValue;
        }

        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line[0])));
        if (c == 'y') {
            return true;
        }
        if (c == 'n') {
            return false;
        }

        std::cout << "Please answer y or n, or press Enter for the default.\n";
    }
}

// -----------------------------------------------------------------------------
// Generic vector helpers
// -----------------------------------------------------------------------------
// These helpers keep the card-moving code readable.
template <typename T>
T takeAt(std::vector<T>& items, std::size_t index) {
    T value = items.at(index);
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
    return value;
}

// -----------------------------------------------------------------------------
// Deck management
// -----------------------------------------------------------------------------
std::vector<Card> buildDeck() {
    std::vector<Card> deck;
    deck.reserve(52);

    for (Suit suit : {Suit::Clubs, Suit::Diamonds, Suit::Hearts, Suit::Spades}) {
        for (int rank = 2; rank <= 14; ++rank) {
            deck.push_back(Card{suit, rank});
        }
    }

    return deck;
}

void shuffleDeck(std::vector<Card>& deck, std::mt19937& rng) {
    std::shuffle(deck.begin(), deck.end(), rng);
}

// Draw one card from the draw deck, reshuffling the discard pile if needed.
// This follows the ruleset's instruction that the discard pile can be recycled
// into a new draw deck only when the draw deck runs out.
std::optional<Card> drawOne(Game& game) {
    if (game.drawDeck.empty()) {
        if (game.discardPile.empty()) {
            return std::nullopt;
        }

        game.drawDeck = game.discardPile;
        game.discardPile.clear();
        shuffleDeck(game.drawDeck, game.rng);
        std::cout << "The discard pile is shuffled back into a fresh draw deck.\n";
    }

    Card top = game.drawDeck.back();
    game.drawDeck.pop_back();
    return top;
}

// Draw a fixed number of cards into a player's hand.
void drawCards(Game& game, Player& player, int count) {
    for (int i = 0; i < count; ++i) {
        std::optional<Card> drawn = drawOne(game);
        if (!drawn.has_value()) {
            std::cout << "No card could be drawn. The deck and discard pile are both empty.\n";
            return;
        }
        player.hand.push_back(*drawn);
    }
}

// -----------------------------------------------------------------------------
// Printing helpers
// -----------------------------------------------------------------------------
std::string handSummary(const std::vector<Card>& hand) {
    if (hand.empty()) {
        return "(empty)";
    }

    std::ostringstream out;
    for (std::size_t i = 0; i < hand.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << "[" << (i + 1) << "] " << cardToString(hand[i]) << " (" << cardValue(hand[i]) << ")";
    }
    return out.str();
}

int totalCardValue(const std::vector<Card>& cards) {
    int total = 0;
    for (const Card& card : cards) {
        total += cardValue(card);
    }
    return total;
}

void showPublicTableState(const Game& game, int kingIndex) {
    std::cout << "\n=== Public Table State ===\n";
    std::cout << "King: " << game.players.at(static_cast<std::size_t>(kingIndex)).name << "\n";
    std::cout << "Draw deck: " << game.drawDeck.size() << " cards\n";
    std::cout << "Discard pile: " << game.discardPile.size() << " cards\n";
    std::cout << "King's favor pile: " << game.favorPile.size() << " cards\n";

    std::cout << "Scores:\n";
    for (const Player& player : game.players) {
        std::cout << "  " << player.name << ": " << player.totalScore << " total";
        if (!player.reignScores.empty()) {
            std::cout << " | best reign " << *std::max_element(player.reignScores.begin(), player.reignScores.end());
        }
        std::cout << "\n";
    }
}

void showPrivateHand(const Player& player) {
    std::cout << "\n" << player.name << ", this is your private hand:\n";
    std::cout << handSummary(player.hand) << "\n";
}

// -----------------------------------------------------------------------------
// Selection helpers
// -----------------------------------------------------------------------------
int chooseCardIndexFromHand(const Player& player, const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line = readLine();

        int choice = 0;
        std::stringstream ss(line);
        char extra = '\0';
        if (ss >> choice && !(ss >> extra)) {
            if (choice >= 1 && choice <= static_cast<int>(player.hand.size())) {
                return choice - 1;
            }
        }

        std::cout << "Please choose a valid card number.\n";
    }
}

int choosePlayerIndex(const Game& game, const std::vector<int>& candidates, const std::string& prompt) {
    while (true) {
        std::cout << prompt << "\n";
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            int playerIndex = candidates[i];
            std::cout << "  [" << (i + 1) << "] " << game.players.at(static_cast<std::size_t>(playerIndex)).name << "\n";
        }

        std::string line = readLine();
        int choice = 0;
        std::stringstream ss(line);
        char extra = '\0';
        if (ss >> choice && !(ss >> extra)) {
            if (choice >= 1 && choice <= static_cast<int>(candidates.size())) {
                return candidates[static_cast<std::size_t>(choice - 1)];
            }
        }

        std::cout << "Please choose one of the listed numbers.\n";
    }
}

// -----------------------------------------------------------------------------
// Round / reign bookkeeping
// -----------------------------------------------------------------------------
std::vector<int> subjectOrderClockwise(int kingIndex, int playerCount) {
    std::vector<int> order;
    for (int step = 1; step < playerCount; ++step) {
        order.push_back((kingIndex + step) % playerCount);
    }
    return order;
}

// A parlay is one of the most important rule interactions in the game.
// The code below follows the ruleset step by step, with a lot of commentary
// around the state changes so the flow is easier to follow if you are new to C++.
void resolveParlay(Game& game,
                   int kingIndex,
                   int donorIndex,
                   int interceptorIndex,
                   Card donationCard,
                   bool rebellionActive,
                   std::optional<int> rebelLeader) {
    Player& interceptor = game.players.at(static_cast<std::size_t>(interceptorIndex));
    Player& defender = game.players.at(static_cast<std::size_t>(donorIndex));
    Player& king = game.players.at(static_cast<std::size_t>(kingIndex));

    std::cout << "\nParlay declared by " << interceptor.name << " against " << defender.name
              << "'s donation to " << king.name << ".\n";

    std::uniform_int_distribution<int> die(1, 6);
    int interceptorRoll = die(game.rng);
    int defenderRoll = die(game.rng);

    std::cout << interceptor.name << " rolls " << interceptorRoll << ".\n";
    std::cout << defender.name << " rolls " << defenderRoll << ".\n";

    if (defenderRoll > interceptorRoll) {
        std::cout << defender.name << " has the higher roll and may cancel the parlay by discarding one card.\n";
        if (!defender.hand.empty() &&
            promptYesNo(defender.name + ", do you want to discard one card to cancel the parlay? [y/N] ", false)) {
            int discardIndex = chooseCardIndexFromHand(defender, defender.name + ", choose a card to discard: ");
            Card discarded = takeAt(defender.hand, static_cast<std::size_t>(discardIndex));
            game.discardPile.push_back(discarded);
            defender.hand.push_back(donationCard);
            std::cout << defender.name << " cancels the parlay by discarding " << cardToString(discarded) << ".\n";
            return;
        }
        std::cout << defender.name << " declines the cancel option or cannot pay it.\n";
    }

    int bustPoint = interceptorRoll + defenderRoll;
    std::cout << "Bust point is " << bustPoint << ".\n";

    if (interceptor.hand.empty() || defender.hand.empty()) {
        // This should almost never happen in legal play, but the guard keeps the
        // program from crashing if the user forces an odd edge case.
        std::cout << "A player lacks a required parlay card. The donation is returned to the defender.\n";
        defender.hand.push_back(donationCard);
        return;
    }

    int interceptorCardIndex = chooseCardIndexFromHand(interceptor,
        interceptor.name + ", choose a card for the parlay: ");

    bool interceptorFaceUp = promptYesNo(interceptor.name + ", play it face up? [y/N] ", false);
    Card interceptorCard = takeAt(interceptor.hand, static_cast<std::size_t>(interceptorCardIndex));

    int defenderCardIndex = chooseCardIndexFromHand(defender,
        defender.name + ", choose a face-down parlay card: ");
    Card defenderCard = takeAt(defender.hand, static_cast<std::size_t>(defenderCardIndex));

    std::cout << interceptor.name << " plays " << cardToString(interceptorCard)
              << (interceptorFaceUp ? " face up" : " face down") << ".\n";
    std::cout << defender.name << " plays a face-down card.\n";

    std::cout << "Reveal: " << interceptor.name << " -> " << cardToString(interceptorCard)
              << ", " << defender.name << " -> " << cardToString(defenderCard) << ".\n";

    int interceptorValue = cardValue(interceptorCard);
    int defenderValue = cardValue(defenderCard);
    bool interceptorOver = interceptorValue > bustPoint;
    bool defenderOver = defenderValue > bustPoint;

    auto discardParlayCards = [&]() {
        game.discardPile.push_back(interceptorCard);
        game.discardPile.push_back(defenderCard);
    };

    if (interceptorValue == defenderValue) {
        std::cout << "It is a tie on card value. Both parlay cards are discarded and the donation returns to the defender.\n";
        discardParlayCards();
        defender.hand.push_back(donationCard);
        return;
    }

    if (interceptorOver && defenderOver) {
        std::cout << "Both players busted. The donation and both parlay cards are discarded.\n";
        discardParlayCards();
        game.discardPile.push_back(donationCard);
        return;
    }

    if (!interceptorOver && !defenderOver) {
        std::cout << "Both players are under the bust point. Both parlay cards return to hand, then each draws one.\n";
        interceptor.hand.push_back(interceptorCard);
        defender.hand.push_back(defenderCard);

        std::optional<Card> interceptorDraw = drawOne(game);
        std::optional<Card> defenderDraw = drawOne(game);
        if (interceptorDraw.has_value()) {
            interceptor.hand.push_back(*interceptorDraw);
            std::cout << interceptor.name << " draws " << cardToString(*interceptorDraw) << ".\n";
        } else {
            std::cout << interceptor.name << " cannot draw because no cards remain.\n";
        }
        if (defenderDraw.has_value()) {
            defender.hand.push_back(*defenderDraw);
            std::cout << defender.name << " draws " << cardToString(*defenderDraw) << ".\n";
        } else {
            std::cout << defender.name << " cannot draw because no cards remain.\n";
        }

        defender.hand.push_back(donationCard);
        return;
    }

    if (interceptorOver && !defenderOver) {
        std::cout << interceptor.name << " wins the parlay.\n";
        discardParlayCards();
        if (rebellionActive) {
            if (rebelLeader.has_value()) {
                Player& leader = game.players.at(static_cast<std::size_t>(*rebelLeader));
                std::cout << "Rebellion is active. Give the intercepted donation to "
                          << leader.name << " instead of keeping it?\n";
                if (promptYesNo("  Give it to the Rebel Leader? [y/N] ", false)) {
                    leader.hand.push_back(donationCard);
                    std::cout << leader.name << " receives the intercepted donation.\n";
                    return;
                }
            }
        }
        interceptor.hand.push_back(donationCard);
        std::cout << interceptor.name << " takes the donated card into hand.\n";
        return;
    }

    if (!interceptorOver && defenderOver) {
        std::cout << defender.name << " wins the parlay.\n";
        discardParlayCards();
        game.favorPile.push_back(donationCard);
        std::cout << "The donation survives and goes to the King's favor pile.\n";
        return;
    }

    // This line should be unreachable because the four cases above cover all
    // combinations. It remains as a guard against accidental logic drift.
    defender.hand.push_back(donationCard);
}

// -----------------------------------------------------------------------------
// Donation action
// -----------------------------------------------------------------------------
void resolveDonation(Game& game,
                     int kingIndex,
                     int donorIndex,
                     bool rebellionActive,
                     std::optional<int> rebelLeader) {
    Player& donor = game.players.at(static_cast<std::size_t>(donorIndex));
    Player& king = game.players.at(static_cast<std::size_t>(kingIndex));

    if (donor.hand.empty()) {
        std::cout << donor.name << " has no cards to donate.\n";
        return;
    }

    if (rebellionActive && donorIndex != kingIndex) {
        // A rebel may no longer donate to the current king.
        // We do not track rebel status here directly; the caller blocks the action
        // if the player is a rebel. This message is here as a second safety net.
        (void)rebelLeader;
    }

    showPrivateHand(donor);
    int donationIndex = chooseCardIndexFromHand(donor, donor.name + ", choose a card to donate: ");
    Card donationCard = takeAt(donor.hand, static_cast<std::size_t>(donationIndex));

    std::cout << donor.name << " places a donation face down before " << king.name << ".\n";

    // Clockwise parlay priority begins with the first eligible player after the donor.
    std::vector<int> parlayOrder;
    int playerCount = static_cast<int>(game.players.size());
    for (int step = 1; step < playerCount; ++step) {
        int candidate = (donorIndex + step) % playerCount;
        if (candidate == kingIndex) {
            continue;
        }
        parlayOrder.push_back(candidate);
    }

    for (int challengerIndex : parlayOrder) {
        Player& challenger = game.players.at(static_cast<std::size_t>(challengerIndex));
        if (challenger.hand.empty()) {
            continue;
        }

        bool wantsParlay = promptYesNo(challenger.name + ", do you want to Parlay? [y/N] ", false);
        if (wantsParlay) {
            resolveParlay(game, kingIndex, donorIndex, challengerIndex, donationCard,
                          rebellionActive, rebelLeader);
            return;
        }
    }

    game.favorPile.push_back(donationCard);
    std::cout << "No one parlayed. The donation goes to " << king.name << "'s favor pile.\n";
}

// -----------------------------------------------------------------------------
// Petition action
// -----------------------------------------------------------------------------
void resolvePetition(Game& game, int kingIndex, int subjectIndex, std::vector<bool>& accusedTyranny) {
    Player& subject = game.players.at(static_cast<std::size_t>(subjectIndex));
    Player& king = game.players.at(static_cast<std::size_t>(kingIndex));

    std::cout << "\n" << subject.name << " petitions " << king.name << ".\n";
    std::cout << "1. Accuse tyranny\n";
    std::cout << "2. Ask for a tax\n";
    std::cout << "3. General petition / speech\n";

    int choice = promptInt("Choose a petition: ", 1, 3, 3);
    switch (choice) {
        case 1:
            accusedTyranny.at(static_cast<std::size_t>(subjectIndex)) = true;
            std::cout << subject.name << " publicly accuses " << king.name << " of tyranny.\n";
            break;
        case 2:
            std::cout << subject.name << " asks for a tax, but the King is not forced to act.\n";
            break;
        default:
            std::cout << subject.name << " makes a general petition. Political meaning is left to the table.\n";
            break;
    }
}

// -----------------------------------------------------------------------------
// Rebellion helpers
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Tax action
// -----------------------------------------------------------------------------
void resolveTax(Game& game, int kingIndex) {
    Player& king = game.players.at(static_cast<std::size_t>(kingIndex));

    std::vector<int> validTargets;
    for (int i = 0; i < static_cast<int>(game.players.size()); ++i) {
        if (i != kingIndex && !game.players.at(static_cast<std::size_t>(i)).hand.empty()) {
            validTargets.push_back(i);
        }
    }

    if (validTargets.empty()) {
        std::cout << "No subject has a card available to tax.\n";
        return;
    }

    int targetIndex = choosePlayerIndex(game, validTargets, king.name + ", choose a subject to tax:");
    Player& target = game.players.at(static_cast<std::size_t>(targetIndex));

    int discardIndex = chooseCardIndexFromHand(target, target.name + ", choose a card to pay the tax: ");
    Card discarded = takeAt(target.hand, static_cast<std::size_t>(discardIndex));
    game.discardPile.push_back(discarded);

    std::cout << king.name << " taxes " << target.name << " for " << cardToString(discarded) << ".\n";
}

// -----------------------------------------------------------------------------
// Royal Coffers / throne transition
// -----------------------------------------------------------------------------
// When a new king takes the throne, the hand they were carrying matters.
// They either bank it for points or open it for the next player in king order.
// The code makes this explicit because it is one of the stranger rule pieces.
int resolveThroneEntry(Game& game, int kingIndex) {
    Player& king = game.players.at(static_cast<std::size_t>(kingIndex));

    if (king.hand.empty()) {
        std::cout << king.name << " arrives at the throne with no hand to manage.\n";
        return 0;
    }

    std::cout << "\n" << king.name << " is about to begin a reign and currently holds a hand.\n";
    showPrivateHand(king);

    std::cout << "1. Bank the Hand\n";
    std::cout << "2. Open the Hand\n";

    int choice = promptInt(king.name + ", choose how to handle your throne hand: ", 1, 2, 1);
    if (choice == 1) {
        int handValue = totalCardValue(king.hand);
        int bonus = handValue / 2;
        std::cout << king.name << " banks the hand for a Royal Coffers bonus of " << bonus << ".\n";
        for (const Card& card : king.hand) {
            game.discardPile.push_back(card);
        }
        king.hand.clear();
        return bonus;
    }

    // Open hand:
    // The next player in king order can take cards from the revealed hand.
    int nextPlayerIndex = (kingIndex + 1) % static_cast<int>(game.players.size());
    Player& nextPlayer = game.players.at(static_cast<std::size_t>(nextPlayerIndex));

    std::cout << king.name << " opens the hand publicly.\n";
    std::cout << nextPlayer.name << " may take any number of cards, limited by both hands.\n";
    std::cout << "Current hand: " << handSummary(king.hand) << "\n";
    std::cout << nextPlayer.name << "'s hand: " << handSummary(nextPlayer.hand) << "\n";

    int maxTake = static_cast<int>(std::min(king.hand.size(), nextPlayer.hand.size()));
    int takeCount = promptInt(nextPlayer.name + ", how many cards do you want to take? ", 0, maxTake, 0);

    for (int i = 0; i < takeCount; ++i) {
        std::cout << "\n" << king.name << "'s revealed hand: " << handSummary(king.hand) << "\n";
        int takeIndex = chooseCardIndexFromHand(king, nextPlayer.name + ", choose a card to take: ");
        Card taken = takeAt(king.hand, static_cast<std::size_t>(takeIndex));
        nextPlayer.hand.push_back(taken);
        std::cout << nextPlayer.name << " takes " << cardToString(taken) << ".\n";

        if (nextPlayer.hand.empty()) {
            std::cout << nextPlayer.name << " has no cards left to discard as payment.\n";
            break;
        }

        int discardIndex = chooseCardIndexFromHand(nextPlayer,
                                                   nextPlayer.name + ", choose a card to discard in payment: ");
        Card discarded = takeAt(nextPlayer.hand, static_cast<std::size_t>(discardIndex));
        game.discardPile.push_back(discarded);
        std::cout << nextPlayer.name << " discards " << cardToString(discarded) << " as payment.\n";
    }

    for (const Card& card : king.hand) {
        game.discardPile.push_back(card);
    }
    king.hand.clear();
    std::cout << "Any unclaimed throne cards are discarded.\n";
    return 0;
}

// -----------------------------------------------------------------------------
// Reign cleanup
// -----------------------------------------------------------------------------
void scoreFavorPile(Game& game, int kingIndex, int reignScore) {
    Player& king = game.players.at(static_cast<std::size_t>(kingIndex));
    reignScore += totalCardValue(game.favorPile);
    king.totalScore += reignScore;
    king.reignScores.push_back(reignScore);

    std::cout << "\n" << king.name << " scores this reign:\n";
    std::cout << "  Favor pile total: " << reignScore << "\n";
    std::cout << "  Total score now: " << king.totalScore << "\n";
}

void clearFavorPileToDiscard(Game& game) {
    for (const Card& card : game.favorPile) {
        game.discardPile.push_back(card);
    }
    game.favorPile.clear();
}

// -----------------------------------------------------------------------------
// Reign action loop
// -----------------------------------------------------------------------------
void playReign(Game& game, int kingIndex, int reignNumber, int reignsPerCycle) {
    Player& king = game.players.at(static_cast<std::size_t>(kingIndex));
    std::cout << "\n============================================================\n";
    std::cout << "Reign " << reignNumber << " of the cycle: " << king.name << " is King.\n";
    std::cout << "============================================================\n";

    int reignScore = resolveThroneEntry(game, kingIndex);

    // These state flags are per-reign, not global to the entire game.
    std::vector<bool> accusedTyranny(game.players.size(), false);
    bool rebellionActive = false;
    std::optional<int> rebelLeader;
    std::vector<bool> rebelFlags(game.players.size(), false);

    for (int round = 1; round <= reignsPerCycle; ++round) {
        std::cout << "\n--- Subject round " << round << " ---\n";

        bool taxUsedThisRound = false;
        std::vector<int> order = subjectOrderClockwise(kingIndex, static_cast<int>(game.players.size()));

        for (int subjectIndex : order) {
            Player& subject = game.players.at(static_cast<std::size_t>(subjectIndex));

            std::cout << "\nIt is " << subject.name << "'s turn.\n";
            std::cout << "Hand size: " << subject.hand.size() << "\n";

            // Rebels can still act, but they cannot donate to the current king.
            if (rebelFlags.at(static_cast<std::size_t>(subjectIndex))) {
                std::cout << subject.name << " is part of the rebellion this reign.\n";
            }

            std::cout << "1. Donate\n";
            std::cout << "2. Draw\n";
            std::cout << "3. Petition\n";
            std::cout << "4. Declare Rebellion\n";
            std::cout << "5. Pass\n";

            bool turnComplete = false;
            while (!turnComplete) {
                int choice = promptInt(subject.name + ", choose an action: ", 1, 5, 5);

                if (choice == 1) {
                    if (rebelFlags.at(static_cast<std::size_t>(subjectIndex))) {
                        std::cout << "Rebels may no longer donate to the current King.\n";
                        continue;
                    }
                    resolveDonation(game, kingIndex, subjectIndex, rebellionActive, rebelLeader);
                    turnComplete = true;
                    continue;
                }

                if (choice == 2) {
                    std::optional<Card> drawn = drawOne(game);
                    if (drawn.has_value()) {
                        subject.hand.push_back(*drawn);
                        std::cout << subject.name << " draws " << cardToString(*drawn) << ".\n";
                    } else {
                        std::cout << "No cards can be drawn right now.\n";
                    }
                    turnComplete = true;
                    continue;
                }

                if (choice == 3) {
                    resolvePetition(game, kingIndex, subjectIndex, accusedTyranny);
                    turnComplete = true;
                    continue;
                }

                if (choice == 4) {
                    if (std::count(accusedTyranny.begin(), accusedTyranny.end(), true) < 3) {
                        std::cout << "Rebellion is not yet legal. At least three subjects must publicly accuse tyranny this reign.\n";
                    } else {
                        rebellionActive = true;
                        rebelFlags.at(static_cast<std::size_t>(subjectIndex)) = true;
                        std::cout << subject.name << " declares rebellion against " << king.name << "!\n";
                        if (!rebelLeader.has_value()) {
                            std::vector<int> leaderCandidates;
                            for (int i = 0; i < static_cast<int>(game.players.size()); ++i) {
                                if (i != kingIndex) {
                                    leaderCandidates.push_back(i);
                                }
                            }

                            if (promptYesNo("Name a Rebel Leader now? [y/N] ", false)) {
                                int leaderIndex = choosePlayerIndex(game, leaderCandidates,
                                    "Choose the Rebel Leader from the subjects:");
                                rebelLeader = leaderIndex;
                                std::cout << game.players.at(static_cast<std::size_t>(leaderIndex)).name
                                          << " is named Rebel Leader.\n";
                            }
                        }
                        turnComplete = true;
                    }
                    continue;
                }

                std::cout << subject.name << " passes.\n";
                turnComplete = true;
            }
        }

        // The king may levy at most one tax per full subject round.
        if (!taxUsedThisRound) {
            if (promptYesNo(king.name + ", do you want to levy a tax this round? [y/N] ", false)) {
                resolveTax(game, kingIndex);
                taxUsedThisRound = true;
            }
        }

        showPublicTableState(game, kingIndex);

        if (std::count(accusedTyranny.begin(), accusedTyranny.end(), true) >= 3) {
            std::cout << "\nThe court considers " << king.name << " Tyrannical this reign.\n";
        }
    }

    scoreFavorPile(game, kingIndex, reignScore);
    clearFavorPileToDiscard(game);

    // The outgoing king has no active hand after the reign, so they draw a new
    // five-card hand to carry into future subject turns.
    Player& outgoingKing = game.players.at(static_cast<std::size_t>(kingIndex));
    if (outgoingKing.hand.empty()) {
        drawCards(game, outgoingKing, 5);
        std::cout << outgoingKing.name << " draws a new 5-card hand for the next time they are a subject.\n";
    }
}

// -----------------------------------------------------------------------------
// End-game resolution
// -----------------------------------------------------------------------------
int highestSingleReignScore(const Player& player) {
    if (player.reignScores.empty()) {
        return 0;
    }
    return *std::max_element(player.reignScores.begin(), player.reignScores.end());
}

std::vector<int> findTopScorers(const Game& game) {
    int topScore = std::max_element(game.players.begin(), game.players.end(),
                                    [](const Player& a, const Player& b) { return a.totalScore < b.totalScore; })
                       ->totalScore;

    std::vector<int> tied;
    for (int i = 0; i < static_cast<int>(game.players.size()); ++i) {
        if (game.players.at(static_cast<std::size_t>(i)).totalScore == topScore) {
            tied.push_back(i);
        }
    }
    return tied;
}

std::vector<int> resolveTieByBestReign(const Game& game, const std::vector<int>& tiedPlayers) {
    int bestReign = -1;
    std::vector<int> best;

    for (int playerIndex : tiedPlayers) {
        int playerBest = highestSingleReignScore(game.players.at(static_cast<std::size_t>(playerIndex)));
        if (playerBest > bestReign) {
            bestReign = playerBest;
            best.clear();
            best.push_back(playerIndex);
        } else if (playerBest == bestReign) {
            best.push_back(playerIndex);
        }
    }

    return best;
}

// -----------------------------------------------------------------------------
// Final Kingless Parlay duel
// -----------------------------------------------------------------------------
// This is a simplified but faithful implementation of the optional final duel.
// If the game ends in a tie after the standard score and single-reign checks,
// the tied players can settle it with a small face-down card showdown.
int runFinalParlayDuel(Game& game, const std::vector<int>& tiedPlayers) {
    std::cout << "\nFinal tie remains. Starting the optional Kingless Parlay duel.\n";
    std::vector<int> duelists = tiedPlayers;

    while (true) {
        for (std::size_t i = 0; i < duelists.size(); ++i) {
            Player& player = game.players.at(static_cast<std::size_t>(duelists[i]));
            for (const Card& card : player.hand) {
                game.discardPile.push_back(card);
            }
            player.hand.clear();
            drawCards(game, player, 5);
            std::cout << player.name << " draws 5 duel cards.\n";
        }

        std::vector<Card> chosenCards;
        chosenCards.reserve(duelists.size());

        for (std::size_t i = 0; i < duelists.size(); ++i) {
            Player& player = game.players.at(static_cast<std::size_t>(duelists[i]));
            showPrivateHand(player);
            int idx = chooseCardIndexFromHand(player, player.name + ", choose one card for the duel: ");
            Card chosen = takeAt(player.hand, static_cast<std::size_t>(idx));
            chosenCards.push_back(chosen);
        }

        std::uniform_int_distribution<int> die(1, 6);
        int bustPoint = die(game.rng) + die(game.rng);
        std::cout << "Bust point for the duel is " << bustPoint << ".\n";

        int bestOverIndex = -1;
        int bestOverDistance = std::numeric_limits<int>::max();
        int bestUnderIndex = -1;
        int bestUnderValue = std::numeric_limits<int>::min();
        bool hasOver = false;
        bool hasUnder = false;

        for (std::size_t i = 0; i < duelists.size(); ++i) {
            int value = cardValue(chosenCards[i]);
            std::cout << game.players.at(static_cast<std::size_t>(duelists[i])).name
                      << " reveals " << cardToString(chosenCards[i]) << " (" << value << ").\n";

            if (value > bustPoint) {
                hasOver = true;
                int distance = value - bustPoint;
                if (distance < bestOverDistance) {
                    bestOverDistance = distance;
                    bestOverIndex = static_cast<int>(i);
                } else if (distance == bestOverDistance) {
                    bestOverIndex = -2; // tied over result
                }
            } else {
                hasUnder = true;
                if (value > bestUnderValue) {
                    bestUnderValue = value;
                    bestUnderIndex = static_cast<int>(i);
                } else if (value == bestUnderValue) {
                    bestUnderIndex = -2; // tied under result
                }
            }
        }

        if (hasOver) {
            if (bestOverIndex >= 0) {
                return duelists[static_cast<std::size_t>(bestOverIndex)];
            }
        } else if (hasUnder) {
            if (bestUnderIndex >= 0) {
                return duelists[static_cast<std::size_t>(bestUnderIndex)];
            }
        }

        std::cout << "The duel is still tied. Repeat.\n";
    }
}

// -----------------------------------------------------------------------------
// Game setup and main loop
// -----------------------------------------------------------------------------
Game createGame() {
    Game game{
        .config = {},
        .players = {},
        .drawDeck = buildDeck(),
        .discardPile = {},
        .favorPile = {},
        .rng = std::mt19937{std::random_device{}()},
    };

    std::cout << "Enter the five player names. Press Enter to keep the default name.\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "Player " << (i + 1) << " name: ";
        std::string name = readLine();
        if (name.empty()) {
            name = "Player " + std::to_string(i + 1);
        }
        game.players.push_back(Player{.name = name});
    }

    game.config.cycles = promptInt("How many cycles do you want to play? [1-3, default 1] ", 1, 3, 1);
    game.config.reignTurnsPerSubject = promptInt("How many turns should each subject get per reign? [1-6, default 3] ",
                                                1, 6, 3);

    shuffleDeck(game.drawDeck, game.rng);

    // Five-card draw style setup: deal 5 to each player, then turn the rest
    // of the deck into the draw deck.
    for (Player& player : game.players) {
        drawCards(game, player, 5);
    }

    return game;
}

int chooseInitialKing(Game& game) {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(game.players.size()) - 1);
    return dist(game.rng);
}

} // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Game game = createGame();
    int kingIndex = chooseInitialKing(game);

    std::cout << "\nA random first King has been chosen: "
              << game.players.at(static_cast<std::size_t>(kingIndex)).name << ".\n";
    std::cout << "King order proceeds clockwise from there.\n";

    int reignNumber = 0;

    for (int cycle = 1; cycle <= game.config.cycles; ++cycle) {
        std::cout << "\n==================== Cycle " << cycle << " ====================\n";
        for (int reignInCycle = 1; reignInCycle <= static_cast<int>(game.players.size()); ++reignInCycle) {
            ++reignNumber;
            playReign(game, kingIndex, reignNumber, game.config.reignTurnsPerSubject);
            kingIndex = (kingIndex + 1) % static_cast<int>(game.players.size());
        }
    }

    std::cout << "\n==================== Final Scores ====================\n";
    for (const Player& player : game.players) {
        int bestReign = highestSingleReignScore(player);
        std::cout << player.name << ": total " << player.totalScore
                  << ", best reign " << bestReign << "\n";
    }

    std::vector<int> topScorers = findTopScorers(game);
    if (topScorers.size() == 1) {
        std::cout << "\nWinner: " << game.players.at(static_cast<std::size_t>(topScorers.front())).name << "\n";
        return 0;
    }

    std::vector<int> bestReignTies = resolveTieByBestReign(game, topScorers);
    if (bestReignTies.size() == 1) {
        std::cout << "\nWinner by best single reign: "
                  << game.players.at(static_cast<std::size_t>(bestReignTies.front())).name << "\n";
        return 0;
    }

    std::cout << "\nThere is still a tie after the single-reign tiebreaker.\n";
    if (promptYesNo("Play the optional final Kingless Parlay duel? [y/N] ", false)) {
        int winnerIndex = runFinalParlayDuel(game, bestReignTies);
        std::cout << "\nWinner: " << game.players.at(static_cast<std::size_t>(winnerIndex)).name << "\n";
    } else {
        std::cout << "Shared victory between:\n";
        for (int index : bestReignTies) {
            std::cout << "  " << game.players.at(static_cast<std::size_t>(index)).name << "\n";
        }
    }

    return 0;
}
