#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <rapidjson/document.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <random>
#include <iomanip>

struct SpriteData {
    std::string name;
    sf::IntRect rect;
};

class AudioManager {
public:

    bool loadAudio() {
        if (!backgroundMusic.openFromFile("Media/MainTheme.wav")) {
            std::cerr << "Failed to load background music\n";
            return false;
        }
        if (!introMusic.openFromFile("Media/INTRO.ogg")) {
            std::cerr << "Failed to load intro music\n";
            return false;
        }
        if (!victoryBuffer.loadFromFile("Media/correct.wav")) {
            std::cerr << "Failed to load victory sound\n";
            return false;
        }
        if (!failBuffer.loadFromFile("Media/error1.wav")) {
            std::cerr << "Failed to load fail sound\n";
            return false;
        }        
        if (!pressBuffer.loadFromFile("Media/press.wav")) {
            std::cerr << "Failed to load fail sound\n";
            return false;
        }


        victorySound.setBuffer(victoryBuffer);
        victorySound.setVolume(110.0f);
        failSound.setBuffer(failBuffer);
        failSound.setVolume(110.0f);        
        pressSound.setBuffer(pressBuffer);
        pressSound.setVolume(50);
        
        return true;
    }

    void playBackgroundMusic() {
        backgroundMusic.setLoop(true);
        introMusic.stop();
        backgroundMusic.setVolume(10.0f);
        backgroundMusic.play();
    }

    void playIntroMusic() {
        introMusic.setLoop(false);
        //introMusic.setPitch(0.8f);
        backgroundMusic.stop();
        introMusic.setVolume(60.0f);
        introMusic.play();
    }

    void playVictorySound() {
        victorySound.play();
    }

    void playFailSound() {
        failSound.play();
    }    
    
    void playPressSound() {
        pressSound.play();
    }

private:
    sf::Music backgroundMusic;
    sf::Music introMusic;
    sf::SoundBuffer victoryBuffer;
    sf::SoundBuffer failBuffer;
    sf::SoundBuffer pressBuffer;
    sf::Sound victorySound;
    sf::Sound failSound;
    sf::Sound pressSound;
};

class TextureManager {
public:
    std::atomic<bool> loadingComplete = false;

    void loadAllAtlases(int atlasCount, const std::string& basePath) {
        for (int i = 1; i <= atlasCount; ++i) {
            std::string textureFile = basePath + "/Atlas" + std::to_string(i) + ".png";
            std::string jsonFile = basePath + "/Atlas" + std::to_string(i) + ".json";

            sf::Texture texture;
            if (!texture.loadFromFile(textureFile)) {
                std::cerr << "Failed to load texture: " << textureFile << "\n";
                return;
            }

            std::vector<SpriteData> spriteData = loadSpriteData(jsonFile);
            if (spriteData.empty()) {
                std::cerr << "Failed to load sprite data: " << jsonFile << "\n";
                return;
            }

            textures.push_back(std::move(texture));
            atlasSpriteData.push_back(spriteData);

            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        loadingComplete = true;
    }

    bool isComplete() { return loadingComplete; }
    sf::Texture& getTexture(size_t index) { return textures[index]; }
    const std::vector<SpriteData>& getSprites(size_t index) const { return atlasSpriteData[index]; }
    size_t getAtlasCount() const { return textures.size(); }

private:
    std::vector<sf::Texture> textures;
    std::vector<std::vector<SpriteData>> atlasSpriteData;

    std::vector<SpriteData> loadSpriteData(const std::string& filename) {
        std::vector<SpriteData> sprites;
        std::ifstream file(filename);
        if (!file) {
            std::cerr << "Failed to open " << filename << "\n";
            return sprites;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        rapidjson::Document document;
        document.Parse(buffer.str().c_str());

        if (!document.IsObject() || !document.HasMember("frames")) {
            std::cerr << "Invalid JSON format\n";
            return sprites;
        }

        for (auto& member : document["frames"].GetObject()) {
            const auto& frame = member.value["frame"];
            SpriteData sprite;
            sprite.name = member.name.GetString();
            sprite.rect = sf::IntRect(frame["x"].GetInt(), frame["y"].GetInt(),
                frame["w"].GetInt(), frame["h"].GetInt());
            sprites.push_back(sprite);
        }
        return sprites;
    }
};

class StratagemGame {
public:
    StratagemGame(AudioManager& audioMgr, TextureManager& txtMgr) : audioManager(audioMgr) , textureManager(txtMgr) {
        loadBackground();
        loadTextures();
        loadFont();
        generateNewSequence();
    }

    void loadBackground() {
        sf::Vector2f size;
        if (!backgroundTexture.loadFromFile("Media/SuperEarth.png")) {
            std::cerr << "Failed to load background\n";
        }
        background.setTexture(backgroundTexture);
        background.setColor(sf::Color(255, 255, 255, 30));
        background.setScale(0.4, 0.4);
        background.setPosition(674, 150);

        if (!logoTexture.loadFromFile("Media/Logo.png")) {
            std::cerr << "Failed to load logo\n";
        }

        logo.setTexture(logoTexture);
        logo.setColor(sf::Color(234, 239, 44, 150));
        logo.setScale(0.2, 0.2);
        logo.setPosition(746, 20); 

        if (!skullTexture.loadFromFile("Media/Skull.png")) {
            std::cerr << "Failed to load logo\n";
        }

        skull.setTexture(skullTexture);
        //skull.setColor(sf::Color(234, 239, 44, 150));
        skull.setScale(0.15, 0.15);
        skull.setPosition(-50, 920); 

        topBorder.setSize(sf::Vector2f(1890, 10));
        topBorder.setFillColor(sf::Color(255, 255, 255, 50));
        topBorder.setPosition(0, 188);

        bottomBorder.setSize(sf::Vector2f(1890, 10));
        bottomBorder.setFillColor(sf::Color(255, 255, 255, 50));
        bottomBorder.setPosition(0, 880);
    }
    void loadTextures() {
        std::vector<std::string> directions = { "Up", "Down", "Left", "Right" };
        for (const auto& dir : directions) {
            sf::Texture texture;
            if (!texture.loadFromFile("Media/Arrows/" + dir + ".png")) {
                std::cerr << "Failed to load " << dir << " texture\n";
            }
            arrowTextures[dir] = std::move(texture);
        }
    }

    void loadFont() {
        if (!font.loadFromFile("Media/Sansation.ttf")) {
            std::cerr << "Failed to load font\n";
        }

        timerText.setFont(font);
        timerText.setCharacterSize(55);
        timerText.setFillColor(sf::Color::Yellow);
        timerText.setPosition(880, 350); 
    }

    void loadPrompt()
    {
        if (!font.loadFromFile("Media/Sansation.ttf")) {
            std::cerr << "Failed to load font\n";
        }

        promptText.setFont(font);
        promptText.setCharacterSize(50);
        promptText.setFillColor(sf::Color::Yellow);
        promptText.setPosition(220, 940);

        promptText.setString("Press Space to Accidentally Drop a Stratagem on Your Teammates");
    }

    void generateNewSequence() {
        if (!repeatSequence) {
            arrows.clear();
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(4, 8);
            std::vector<std::string> directions = { "Up", "Down", "Left", "Right" };
            
            int arrowCount = 16;
            for (int i = 0; i < arrowCount; ++i) {
                std::uniform_int_distribution<> arrowDist(0, 3);
                std::string direction = directions[arrowDist(gen)];
                sf::Sprite sprite;
                sprite.setTexture(arrowTextures[direction]);
                sprite.setColor(sf::Color::White);
                sprite.setScale(2.5, 2.5);
                sprite.setPosition(155 + i * 100, 500);
                arrows.push_back({ direction, sprite });
            }
        }

        else {
            for (auto& arrow : arrows) {
                arrow.second.setColor(sf::Color::White);
            }
        }
        currentArrowIndex = 0;
        repeatSequence = false;
        timerRunning = false;
        elapsedTime = 0.0f;
    }

    void handleInput(sf::Keyboard::Key key) {
        std::map<sf::Keyboard::Key, std::string> keyMap = {
            {sf::Keyboard::Up, "Up"},
            {sf::Keyboard::Down, "Down"},
            {sf::Keyboard::Left, "Left"},
            {sf::Keyboard::Right, "Right"},
            {sf::Keyboard::W, "Up"},
            {sf::Keyboard::S, "Down"},
            {sf::Keyboard::A, "Left"},
            {sf::Keyboard::D, "Right"},
            {sf::Keyboard::Space, "Space"}
        };
        
        audioManager.playPressSound();

        if (keyMap.count(key) > 0 && currentArrowIndex < arrows.size()) {
            if (!timerRunning) {
                timerRunning = true;
                clock.restart(); // Start timer
            }

            if (keyMap[key] == arrows[currentArrowIndex].first) {
                arrows[currentArrowIndex].second.setColor(sf::Color::Yellow);
                currentArrowIndex++;
                if (currentArrowIndex >= arrows.size()) {
                    elapsedTime = clock.getElapsedTime().asSeconds();
                    audioManager.playVictorySound();
                    generateNewSequence();
                }
            } 
            else {
                arrows[currentArrowIndex].second.setColor(sf::Color::Red);
                repeatSequence = true;
                audioManager.playFailSound();
                generateNewSequence();
            }
        }

        if (keyMap[key] == "Space" && textureManager.isComplete())
        {
            isExiting = true;
        }
    }

    void update(float deltaTime) {
        if (!textureManager.isComplete()) {
            skullAlpha += fadeDirection * deltaTime * 100;
            if (skullAlpha >= 255 || skullAlpha <= 0) {
                fadeDirection *= -1;
            }
            skull.setColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(skullAlpha)));
        }
        else
        {
            loadPrompt();
        }

        if (timerRunning) {
            elapsedTime = clock.getElapsedTime().asSeconds();
            timerText.setString(formatTime(elapsedTime) + "s");
        }
    }

    bool exitGame() { return isExiting; }

    void draw(sf::RenderWindow& window) {
        window.draw(background);
        window.draw(logo);
        window.draw(skull);
        window.draw(topBorder);
        window.draw(bottomBorder);
        for (const auto& arrow : arrows) {
            window.draw(arrow.second);
        }
        window.draw(timerText);
        window.draw(promptText);
    }

private:
    std::map<std::string, sf::Texture> arrowTextures;
    std::vector<std::pair<std::string, sf::Sprite>> arrows;
    size_t currentArrowIndex;
    sf::Sprite arrowSprite;
    std::string currentDirection;

    sf::Texture backgroundTexture;
    sf::Sprite background;

    sf::Texture logoTexture;
    sf::Sprite logo;

    sf::Texture skullTexture;
    sf::Sprite skull;
    float skullAlpha = 0;
    int fadeDirection = 1;

    sf::RectangleShape topBorder;
    sf::RectangleShape bottomBorder;
    bool repeatSequence = false;
    bool assetsLoaded = false;

    sf::Clock clock;
    float elapsedTime = 0.0f;
    bool timerRunning = false;

    sf::Font font;
    sf::Text timerText;
    sf::Text promptText;

    bool isExiting = false;

    AudioManager& audioManager;
    TextureManager& textureManager;

    std::string formatTime(float time) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << time;
        return stream.str();
    }

};

int main() {
    sf::RenderWindow window(sf::VideoMode(1890, 1080), "Interactive Loading Screen");
    window.setFramerateLimit(60);

    TextureManager textureManager;
    std::thread loadingThread(&TextureManager::loadAllAtlases, &textureManager, 45, "Media/Helldivers2");

    AudioManager audioManager;
    if (!audioManager.loadAudio()) {
        return -1;
    }

    size_t currentAtlas = 0;
    size_t currentIndex = 0;
    sf::Sprite sprite;

    sf::Clock frameClock;
    int frameCounter = 0;

    audioManager.playBackgroundMusic();

    StratagemGame stratagemGame(audioManager, textureManager);

    sf::Font font;
    sf::Text fpsText;
    sf::Clock fpsClock;
    int frameCount = 0;
    float fps = 0.0f;

    if (!font.loadFromFile("Media/Sansation.ttf")) {
        std::cerr << "Failed to load font!\n";
        return -1;
    }

    fpsText.setFont(font);
    fpsText.setCharacterSize(55);
    fpsText.setFillColor(sf::Color::White);
    fpsText.setPosition(10, 10);

    while (window.isOpen()) {
        sf::Event event;
        float deltaTime = fpsClock.restart().asSeconds();

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            else if (event.type == sf::Event::KeyPressed)
                stratagemGame.handleInput(event.key.code);
        }

        stratagemGame.update(deltaTime);

        window.clear();

        if (!stratagemGame.exitGame()) {
            stratagemGame.draw(window);
        }
        else {
            if (sprite.getTexture() == nullptr) {
                audioManager.playIntroMusic();
                sprite.setTexture(textureManager.getTexture(currentAtlas));
                sprite.setTextureRect(textureManager.getSprites(currentAtlas)[currentIndex].rect);
                sprite.setScale(3, 3);
                sprite.setPosition(0, 0);
            }

            static int animationFrameCounter = 0;

            animationFrameCounter++;
            if (animationFrameCounter % 2 == 0) { 
                currentIndex++;
                if (currentIndex >= textureManager.getSprites(currentAtlas).size()) {
                    currentIndex = 0;
                    currentAtlas = (currentAtlas + 1) % textureManager.getAtlasCount();
                    sprite.setTexture(textureManager.getTexture(currentAtlas));
                }
                sprite.setTextureRect(textureManager.getSprites(currentAtlas)[currentIndex].rect);
            }

            window.draw(sprite);
        }

        static sf::Clock fpsUpdateClock; 

        frameCount++;

        float elapsedTime = fpsUpdateClock.getElapsedTime().asSeconds();
        if (elapsedTime >= 0.5f) {
            fps = frameCount / elapsedTime;  
            fpsText.setString("FPS: " + std::to_string(static_cast<int>(fps)));

            fpsUpdateClock.restart();
            frameCount = 0;
        }

        window.draw(fpsText); 
        window.display();
    }

    loadingThread.join();

    return 0;
}
