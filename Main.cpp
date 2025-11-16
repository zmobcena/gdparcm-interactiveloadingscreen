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
        if (!introMusic.openFromFile("Media/SM2.wav")) {
            std::cerr << "Failed to load intro music\n";
            return false;
        }

        if (!shootBuffer.loadFromFile("Media/New Assets/Shoot.ogg")) {
            std::cerr << "Failed to load fail sound\n";
            return false;
        }
        if (!hitBuffer.loadFromFile("Media/New Assets/Hit.ogg")) {
            std::cerr << "Failed to load fail sound\n";
            return false;
        }

        hitSound.setBuffer(hitBuffer);
        hitSound.setVolume(40);
        shootSound.setBuffer(shootBuffer);
        shootSound.setVolume(60);
        
        return true;
    }

    void playBackgroundMusic() {
        backgroundMusic.setLoop(true);
        introMusic.stop();
        backgroundMusic.setVolume(40.0f);
        backgroundMusic.play();
    }

    void playIntroMusic() {
        introMusic.setLoop(false);
        backgroundMusic.stop();
        introMusic.setVolume(60.0f);
        introMusic.play();
    }

    void playhitSound() {
        hitSound.play();
    }

    void playshootSound() {
        shootSound.play();
    }
    
    void playPressSound() {
        pressSound.play();
    }

private:
    sf::Music backgroundMusic;
    sf::Music introMusic;
	sf::SoundBuffer shootBuffer;
	sf::SoundBuffer hitBuffer;
    sf::SoundBuffer pressBuffer;
    sf::Sound shootSound;
    sf::Sound hitSound;
    sf::Sound pressSound;
};

class TextureManager {
public:
    std::atomic<bool> loadingComplete = false;

    void loadImagesThreaded(int startIndex,
        int totalImages,
        const std::string& folder,
        int threadCount)
    {
        textures.resize(totalImages);

        int imagesPerThread = totalImages / threadCount;
        int remainder = totalImages % threadCount;

        std::vector<std::thread> threads;
        std::atomic<int> loadedCount = 0;

        auto loadRange = [&](int start, int end)
            {
                for (int i = start; i < end; i++)
                {
                    // The actual index used in the filename
                    int realIndex = startIndex + (i - 1);

                    std::string fileName = folder + "/SM2_" + std::to_string(realIndex) + ".jpg";

                    if (!textures[i - 1].loadFromFile(fileName)) {
                        std::cerr << "Failed to load " << fileName << "\n";
                    }

                    loadedCount++;
                }
            };

        int start = 1;
        for (int t = 0; t < threadCount; t++)
        {
            int count = imagesPerThread + (t < remainder ? 1 : 0);
            int end = start + count;

            threads.emplace_back(loadRange, start, end);
            start = end;
        }

        for (auto& th : threads)
            th.join();

        loadingComplete = true;
    }

    bool isComplete() { return loadingComplete; }
    sf::Texture& getTexture(size_t index) { return textures[index]; }
    size_t getImageCount() const { return textures.size(); }
    size_t getAtlasCount() const { return textures.size(); }

private:
    std::vector<sf::Texture> textures;

};

struct Enemy {
    sf::Sprite sprite;
    float lifeTime = 0.0f;     
    float spawnTime = 0.0f;      
    bool alive = true;
};


class ShooterGame {
public:
    ShooterGame(AudioManager& audioMgr, TextureManager& txtMgr, sf::RenderWindow& window)
        : audioManager(audioMgr), textureManager(txtMgr), renderWindow(window)
    {
        camera = window.getDefaultView();
        originalCenter = camera.getCenter();
        loadBackground();
        loadTextures();
        initCrosshair();

        windowSize = sf::Vector2f(1920, 1080);

        sf::Mouse::setPosition(sf::Vector2i(windowSize.x / 2, windowSize.y / 2));

		if (!font.loadFromFile("Media/Qonquer.otf")) {
			std::cerr << "Failed to load font!\n";
		}

        if (!skullTexture.loadFromFile("Media/Skull.png")) {
            std::cerr << "Failed to load logo\n";
        }

        skull.setTexture(skullTexture);
        skull.setOrigin(
            skullTexture.getSize().x / 2,
            skullTexture.getSize().y / 2
        );
        skull.setScale(0.8, 0.8);
        skull.setPosition(120, 950);

        scoreText.setFont(font);
        scoreText.setCharacterSize(55);
        scoreText.setFillColor(sf::Color::White);
        scoreText.setPosition(1650, 10);
        scoreText.setOutlineColor(sf::Color::Black);       
        scoreText.setOutlineThickness(1.6f);              

		reactionTimeText.setFont(font);
		reactionTimeText.setCharacterSize(35);
		reactionTimeText.setFillColor(sf::Color::White);
        reactionTimeText.setOutlineColor(sf::Color::Black);
        reactionTimeText.setOutlineThickness(1.6f);

		hpText.setFont(font);
		hpText.setCharacterSize(60);
		hpText.setString("HP");
		hpText.setFillColor(sf::Color::White);
		hpText.setPosition(50, 2);
        hpText.setOutlineColor(sf::Color::Black);
        hpText.setOutlineThickness(1.6f);
		 
        healthBarBack.setSize(sf::Vector2f(healthWidth, healthHeight));
        healthBarBack.setFillColor(sf::Color(80, 80, 80));  
        healthBarBack.setPosition(127, 14);

        healthBarFront.setSize(sf::Vector2f(healthWidth, healthHeight));
        healthBarFront.setFillColor(sf::Color(200, 50, 50)); 
        healthBarFront.setPosition(127, 14);

        healthbar.setScale(0.15, 0.15);
        healthbar.setPosition(380, 38);
    }

    void loadBackground() {
        sf::Vector2f size;
        if (!backgroundTexture.loadFromFile("Media/New Assets/sm2bg.jpg")) {
            std::cerr << "Failed to load background\n";
        }
        background.setTexture(backgroundTexture);
        //background.setColor(sf::Color(255, 255, 255, 100));
        background.setScale(0.75, 0.75);
        background.setPosition(0, 0);
	}

    void loadPrompt()
    {
        if (!font.loadFromFile("Media/Qonquer.otf")) {
            std::cerr << "Failed to load font\n";
        }

        promptText.setFont(font);
        promptText.setCharacterSize(50);
        promptText.setFillColor(sf::Color::Yellow);
        promptText.setPosition(220, 940);

        promptText.setString("Press Space to Continue");
    }

    void loadTextures() {
        if (!enemyTexture.loadFromFile("Media/New Assets/Enemy4.png")) {
            std::cerr << "Failed to load enemy texture\n";
        }

        if (!healthbarTexture.loadFromFile("Media/New Assets/HealthBar.png")) {
            std::cerr << "Failed to load crosshair texture\n";
        }

        healthbar.setTexture(healthbarTexture);
        healthbar.setOrigin(
            healthbarTexture.getSize().x / 2,
            healthbarTexture.getSize().y / 2
        );
    }

    void update(float deltaTime, sf::RenderWindow& window)
    {
        if (shaking) {
            shakeDuration -= deltaTime;
            if (shakeDuration > 0.f) {
                float offsetX = (rand() % 100 / 100.f - 0.5f) * shakeStrength;
                float offsetY = (rand() % 100 / 100.f - 0.5f) * shakeStrength;
                camera.setCenter(originalCenter.x + offsetX,
                    originalCenter.y + offsetY);
            }
            else {
                shaking = false;
                camera.setCenter(originalCenter);
            }
        }

        window.setView(camera); // apply camera every frame

        updateCrosshair(deltaTime);

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

        totalTime += deltaTime;
        spawnTimer += deltaTime;
        if (spawnTimer >= spawnInterval) {
            spawnEnemy();

            spawnTimer = 0;
        }

        spawnInterval -= 0.0001f;

        for (auto& enemy : enemies) {
            if (!enemy.alive) continue;

            enemy.lifeTime += deltaTime;

            if (enemy.lifeTime >= 2.0f) {
                enemy.alive = false;
                currentHealth -= 10;     
                
            }
        }

        if (currentHealth < 0) {
            currentHealth = 0;
            currentHealth = maxHealth;
			score = 0;
			spawnInterval = 1.0f;
        }

        enemies.erase(
            std::remove_if(enemies.begin(), enemies.end(),
                [](const Enemy& e) { return !e.alive; }),
            enemies.end()
        );

        float ratio = static_cast<float>(currentHealth) / maxHealth;
        healthBarFront.setSize(sf::Vector2f(healthWidth * ratio, healthHeight));

        scoreText.setString("Score: " + std::to_string(score));

        updateCrosshair(window);
    }


    void draw(sf::RenderWindow& window) {
        window.draw(background);

        for (auto& enemy : enemies) {
            if (enemy.alive)
                window.draw(enemy.sprite);
        }

		window.draw(hpText);

		window.draw(reactionTimeText);

        window.draw(healthBarBack);
        window.draw(healthBarFront);
        window.draw(healthbar);

        window.draw(scoreText);

        window.draw(skull);
        window.draw(promptText);

        window.draw(topBar);
        window.draw(bottomBar);
        window.draw(leftBar);
        window.draw(rightBar);
    }

    void handleMouseClick(sf::RenderWindow& window) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        sf::Vector2f worldPos(mousePos.x, mousePos.y);

        startShake(0.15f, 10.f);
        triggerBloom();
        audioManager.playshootSound();

        for (auto& enemy : enemies) {
            if (!enemy.alive) continue;

            if (enemy.sprite.getGlobalBounds().contains(worldPos)) {

                float reaction = (totalTime - enemy.spawnTime) * 1000; 

                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << reaction;

                reactionTimeText.setString("Reaction: " + ss.str() + " ms");
				float reactionTimeTextPosX = (windowSize.x / 2) - (reactionTimeText.getLocalBounds().width / 2);
                reactionTimeText.setPosition(reactionTimeTextPosX, 20);

                enemy.alive = false;
                audioManager.playhitSound();
                score++;
            }
        }
    }

    void handleInput(sf::Keyboard::Key key) {
        std::map<sf::Keyboard::Key, std::string> keyMap = {
            {sf::Keyboard::Space, "Space"}
        };

        audioManager.playPressSound();

        if (keyMap[key] == "Space" && textureManager.isComplete())
        {
            isExiting = true;
        }
    }

    bool exitGame() { return isExiting; }

private:
    std::vector<Enemy> enemies;
    sf::Texture enemyTexture;

    int healthWidth = 508;
    int healthHeight = 50;
    float spawnInterval = 1.0f;
    float spawnTimer = 0.0f;
	float reactionTimer = 0.0f;
    float totalTime = 0.0f;

	bool isExiting = false;

    sf::Vector2f windowSize;
    sf::Texture healthbarTexture;
    sf::Sprite healthbar;

    int maxHealth = 100;
    int currentHealth = 100;

    sf::RectangleShape healthBarBack;
    sf::RectangleShape healthBarFront;

    sf::Texture skullTexture;
    sf::Sprite skull;
    float skullAlpha = 0;
    int fadeDirection = 1;

    sf::Texture backgroundTexture;
    sf::Sprite background;

    sf::Text reactionTimeText;
    sf::Text hpText;
    sf::Text scoreText;
    sf::Text promptText;
    sf::Font font;

    float crosshairGap = 8.f;          
    float crosshairBloom = 0.f;       
    float bloomMax = 16.f;             
    float bloomReturnSpeed = 60.f;     

    sf::RectangleShape topBar, bottomBar, leftBar, rightBar;

    sf::View camera;

    bool shaking = false;
    float shakeDuration = 0.f;
    float shakeStrength = 0.f;

    sf::Vector2f originalCenter;

    AudioManager& audioManager;
    TextureManager& textureManager;
    sf::RenderWindow& renderWindow;

    int score = 0;

    void spawnEnemy() {
        Enemy e;
        e.sprite.setTexture(enemyTexture);

        e.sprite.setOrigin(
            enemyTexture.getSize().x / 2,
            enemyTexture.getSize().y / 2
        );

        float x = randomFloat(100, windowSize.x - 100);
        float y = randomFloat(200, windowSize.y - 200);

        e.sprite.setPosition(x, y);
        e.sprite.setScale(0.3f, 0.3f);

        e.spawnTime = totalTime;   

        enemies.push_back(e);
    }

    float randomFloat(float min, float max) {
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
    }

    void initCrosshair() {
        topBar.setSize({ 3, 15 });
        bottomBar.setSize({ 3, 15 });
        leftBar.setSize({ 15, 3 });
        rightBar.setSize({ 15, 3 });

        topBar.setFillColor(sf::Color::White);
        bottomBar.setFillColor(sf::Color::White);
        leftBar.setFillColor(sf::Color::White);
        rightBar.setFillColor(sf::Color::White);
    }

    void triggerBloom() {
        crosshairBloom = bloomMax;
    }

    void updateCrosshair(float dt) {
        if (crosshairBloom > 0) {
            crosshairBloom -= bloomReturnSpeed * dt;
            if (crosshairBloom < 0)
                crosshairBloom = 0;
        }
    }

    void updateCrosshair(sf::RenderWindow& window) {
        sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
        sf::Vector2f center(mousePixel.x, mousePixel.y);

        float gap = crosshairGap + crosshairBloom;

        // Vertical bars
        topBar.setPosition(center.x - topBar.getSize().x / 2.f,
            center.y - gap - topBar.getSize().y);

        bottomBar.setPosition(center.x - bottomBar.getSize().x / 2.f,
            center.y + gap);

        // Horizontal bars
        leftBar.setPosition(center.x - gap - leftBar.getSize().x,
            center.y - leftBar.getSize().y / 2.f);

        rightBar.setPosition(center.x + gap,
            center.y - rightBar.getSize().y / 2.f);
    }

    void startShake(float duration, float strength) {
        shaking = true;
        shakeDuration = duration;
        shakeStrength = strength;
    }
};

int main() {
    sf::RenderWindow window(sf::VideoMode(1920, 1080), "Interactive Loading Screen");
    window.setFramerateLimit(60);

    window.setMouseCursorGrabbed(true);
    window.setMouseCursorVisible(false);

    TextureManager textureManager;

    std::thread loadingThread(&TextureManager::loadImagesThreaded, &textureManager, 0, 4100, "Media/SM2_720p", 2);

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

    ShooterGame shooterGame(audioManager, textureManager, window);

    sf::Font font;
    sf::Text fpsText;
    sf::Clock fpsClock;
    int frameCount = 0;
    float fps = 0.0f;

    if (!font.loadFromFile("Media/Qonquer.otf")) {
        std::cerr << "Failed to load font!\n";
        return -1;
    }

    fpsText.setFont(font);
    fpsText.setCharacterSize(55);
    fpsText.setFillColor(sf::Color::White);
    fpsText.setPosition(1760, 1000);
    fpsText.setOutlineColor(sf::Color::Black);
    fpsText.setOutlineThickness(1.6f);

    while (window.isOpen()) {
        sf::Event event;
        float deltaTime = fpsClock.restart().asSeconds();

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            else if (event.type == sf::Event::KeyPressed) {
                //stratagemGame.handleInput(event.key.code);
				shooterGame.handleInput(event.key.code);
            } 
            else if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left)
            {
                shooterGame.handleMouseClick(window);
            }
        }

		shooterGame.update(deltaTime, window);

        window.clear();

        static bool playbackInitialized = false;
        static int currentFrame = 0;

        if (textureManager.isComplete() && shooterGame.exitGame())
        {
            if (!playbackInitialized)
            {
                audioManager.playIntroMusic();

                sprite.setTexture(textureManager.getTexture(0));
                sprite.setScale(1.5, 1.5);
                sprite.setPosition(0, 0);
                playbackInitialized = true;
            }

            // Play image sequence
            static int animationFrameCounter = 0;

            animationFrameCounter++;
            if (animationFrameCounter % 2 == 0)
            {
                currentFrame++;

                if (currentFrame >= textureManager.getImageCount()) {
                    currentFrame = 0; // loop
                }

                sprite.setTexture(textureManager.getTexture(currentFrame));
            }

            window.draw(sprite);
        }
        else
        {
            shooterGame.draw(window);
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

    return 0;
}
