#include "AudioManager.h"

AudioManager* AudioManager::GetInstance() {
	static AudioManager instance;
	return &instance;
}

void AudioManager::RefreshAudioFileList() {
    audioFileList_.clear();

    std::string kAudioDirPath = "resources/audio/"; // ルートとなるフォルダ

    if (!fs::exists(kAudioDirPath)) return;

    // recursive_directory_iterator を使うことでサブフォルダも全て探索する
    for (const auto& entry : fs::recursive_directory_iterator(kAudioDirPath)) {
        // ファイルであり、かつディレクトリではないことを確認
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            // 大文字小文字を区別する場合があるため注意
            if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
                // Windows標準のバックスラッシュ(\)をスラッシュ(/)に統一しておくと事故が減ります
                std::string path = entry.path().string();
                std::replace(path.begin(), path.end(), '\\', '/');
                audioFileList_.push_back(path);
            }
        }
    }
}

// 全ての音設定をロードしてAudioクラスに登録する
void AudioManager::LoadAllConfigs(const std::string& path) {
	std::ifstream file(path);
	if (!file.is_open()) return;

	nlohmann::json j;
	file >> j;

	for (auto& [key, value] : j.items()) {
		AudioConfig config;
		config.FromJson(key, value);
		configs_[key] = config;

		// Audioクラス（エンジン側）に読み込み命令を出す
		// std::string -> std::wstring 変換が必要
		std::wstring wName = ConvertString(key);
		std::wstring wPath = ConvertString(config.filePath);
		Audio::GetInstance()->LoadAudio(wName, wPath, config.maxConcurrency);
	}
}

void AudioManager::UpdateImGui() {


#ifdef USE_IMGUI

    ImGui::Begin("Audio Editor");
    
    // 最初にリストを更新するボタンがあると便利
    if (ImGui::Button("Refresh File List")) {
        RefreshAudioFileList();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add New Audio")) {
        std::string newName = "NewAudio_" + std::to_string(configs_.size());
        configs_[newName] = AudioConfig{ newName, "resources/audio/sample.wav" };
    }

    ImGui::Separator();

    std::string renameFrom = "";
    std::string renameTo = "";

    int i = 0; // ループカウンターを用意
    for (auto& [name, config] : configs_) {
        std::string treeLabel = name + "##" + std::to_string(i++);

        if (ImGui::TreeNode(treeLabel.c_str())) {
            // --- 1. 名前の変更 (Enterキーで確定するように変更) ---
            char nameBuf[64];
            strcpy_s(nameBuf, name.c_str());

            ImGui::Text("Audio ID:");
            ImGui::SameLine();

            // Flagsに ImGuiInputTextFlags_EnterReturnsTrue を追加
            std::string inputLabel = "##inputID" + std::to_string(i);
            if (ImGui::InputText(inputLabel.c_str(), nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (strlen(nameBuf) > 0) { // 空文字でないことを確認
                    renameFrom = name;
                    renameTo = nameBuf;
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(Press Enter)"); // ユーザーにEnterが必要だと伝える

            // --- 2. パスの変更 ---
            ImGui::Text("File Path:");

            // 現在のパスからファイル名だけ抽出してラベルにする（見た目スッキリ）
            std::string currentFileName = fs::path(config.filePath).filename().string();

            if (ImGui::BeginCombo(("##pathCombo" + std::to_string(i)).c_str(), currentFileName.c_str())) {
                for (const auto& filePath : audioFileList_) {
                    std::string fileName = fs::path(filePath).filename().string();
                    bool isSelected = (config.filePath == filePath);

                    if (ImGui::Selectable(fileName.c_str(), isSelected)) {
                        config.filePath = filePath; // パスを更新！
                    }

                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            // --- 3. パラメータの変更 ---
            ImGui::DragFloat("Volume", &config.defaultVolume, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox("Loop", &config.loop);

            if (ImGui::InputInt("Concurrency", &config.maxConcurrency)) {
                if (config.maxConcurrency < 1) config.maxConcurrency = 1;
            }

            // --- 4. 反映とプレビュー ---
            std::wstring wName = ConvertString(name);
            if (ImGui::Button("Preview Play")) {
                if (config.loop) {
                    //Audio::GetInstance()->PlayAudio(wName, config.loop, config.defaultVolume);
                    PlayBGM(name);
                } else {
                    Audio::GetInstance()->PlayAudioSE(wName, config.defaultVolume);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Apply & Reload")) {
                Audio::GetInstance()->UnloadAudio(wName);
                Audio::GetInstance()->LoadAudio(wName, ConvertString(config.filePath), config.maxConcurrency);
            }

            // --- 5. 削除機能 ---
            if (ImGui::Button("Delete This Audio")) {
                Audio::GetInstance()->UnloadAudio(wName);
                renameFrom = name;
                renameTo = "";
            }

            ImGui::TreePop();
        }
    }

    // mapの更新（ループ外で行う）
    if (!renameFrom.empty()) {
        if (renameTo.empty()) {
            configs_.erase(renameFrom);
        } else if (renameFrom != renameTo) {
            configs_[renameTo] = configs_[renameFrom];
            configs_[renameTo].name = renameTo;
            configs_.erase(renameFrom);
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Save Config")) {
        SaveAllConfigs("resources/configs/audioSettings.json");
    }

    ImGui::End();

#endif // USE_IMGUI

}

// 文字列変換用ヘルパー (std::string <-> std::wstring)
std::wstring AudioManager::ConvertString(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

void AudioManager::SaveAllConfigs(const std::string& path) {
	nlohmann::json j;
	for (auto& [name, config] : configs_) {
		j[name] = config.ToJson();
	}
	std::ofstream file(path);
	file << std::setw(4) << j << std::endl;
}

// AudioManager経由でSEを鳴らす
void AudioManager::PlaySE(const std::string& name) {
    auto it = configs_.find(name);
    if (it == configs_.end()) return;

    // エディターで設定した volume を使って再生！
    Audio::GetInstance()->PlayAudioSE(
        ConvertString(name),
        it->second.defaultVolume
    );
}

// BGMも同様
void AudioManager::PlayBGM(const std::string& name) {
    // 1. 設定が存在するかチェック
    auto it = configs_.find(name);
    if (it == configs_.end()) return;

    // 2. 既に同じBGMが流れているなら、何もしない（最初から再生し直したい場合はここを消す）
    if (currentBGMName_ == name) return;

    // 3. 他のBGMが流れていたら停止する
    if (!currentBGMName_.empty()) {
        Audio::GetInstance()->StopAudio(ConvertString(currentBGMName_));
    }

    // 4. 新しいBGMを再生
    // エディターの Loop 設定と Volume 設定をそのまま使う
    Audio::GetInstance()->PlayAudio(
        ConvertString(name),
        it->second.loop,
        it->second.defaultVolume
    );

    // 5. 「今はこの音が流れている」と記憶する
    currentBGMName_ = name;
}

void AudioManager::StopBGM() {
    if (!currentBGMName_.empty()) {
        Audio::GetInstance()->StopAudio(ConvertString(currentBGMName_));
        currentBGMName_ = "";
    }
}