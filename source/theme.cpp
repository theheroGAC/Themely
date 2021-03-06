// Themely - home menu manager
// Copyright (c) 2017 Erman SAYIN

#include "theme.h"

deque<Theme> themes;
bool themesScanned = false;
bool isInstalling = false;
wstring installProgress = L"";

bool deletePrompt = false;
bool dumpPrompt = false;

bool shuffleMode = false;

void loadTheme(void* entryVP){
	FS_DirectoryEntry* entry = static_cast<FS_DirectoryEntry*>(entryVP);

	if(entry->attributes & FS_ATTRIBUTE_DIRECTORY){
		if(fileExists(u"/Themes/" + u16string((char16_t*)entry->name) + u"/body_LZ.bin")){
			Theme theme = {
				u16tstr(entry->name, 0x106),
				u16tstr(entry->name, 0x106),
				ws2s(i18n("no_desc")),
				ws2s(i18n("unknown")),
				NULL,
				NULL,
				false,
				true,
				true,
				false,
				fileExists(u"/Themes/" + u16string((char16_t*)entry->name) + u"/bgm.ogg"),
				fileExists(u"/Themes/" + u16string((char16_t*)entry->name) + u"/Preview.png")
			};

			LightLock_Init(&theme.lock);

			ifstream smdhFile("/Themes/" + theme.fileName + "/info.smdh", ios::in | ios::binary);
			if(smdhFile.is_open()){
				char* buffer = new char[0x520];
				smdhFile.read(buffer, 0x520);

				int offset = 0x8;

				string tmpTitle = "";
				string tmpDescription = "";
				string tmpAuthor = "";

				for (size_t i = 0; i < 128; i++){
					if(buffer[offset + i] == '\00')
						break;

					tmpTitle += buffer[offset + i];
					offset++;
				}

				offset = 0x8 + 128;

				for (size_t i = 0; i < 256; i++){
					if(buffer[offset + i] == '\00')
						break;

					tmpDescription += buffer[offset + i];
					offset++;
				}

				offset = 0x8 + 128 + 256;

				for (size_t i = 0; i < 128; i++){
					if(buffer[offset + i] == '\00')
						break;

					tmpAuthor += buffer[offset + i];
					offset++;
				}

				//if(tmpTitle.find_first_not_of('\00') != string::npos)
				if(tmpTitle.size() != 0)
					theme.title = tmpTitle;

				if(tmpDescription.size() != 0)
					theme.description = tmpDescription;

				if(tmpAuthor.size() != 0 && tmpAuthor.size() < 20)
					theme.author = tmpAuthor;

				smdhFile.seekg(0x24C0);
				char* iconBuf = new char[0x1200];
				smdhFile.read(iconBuf, 0x1200);

				// detects default icons
				if(iconBuf[0] != '\x9D' && iconBuf[1] != '\x04' && iconBuf[0] != '\xBF' && iconBuf[1] != '\x0D'){
					theme.icon = sf2d_create_texture(48, 48, TEXFMT_RGB565, SF2D_PLACE_RAM);
					u16* dst = (u16*)(theme.icon->tex.data + 64 * 8 * 2 * sizeof(u16));
					u16* src = (u16*)(iconBuf);
					for (u8 j = 0; j < 48; j += 8){
						memcpy(dst, src, 48 * 8 * sizeof(u16));
						src += 48 * 8;
						dst += 64 * 8;
					}
				}

				delete[] buffer;
				delete[] iconBuf;
			}

			smdhFile.close();

			themes.push_back(theme);

			if(themes.size() < 3){
				themes[themes.size()-1].previewIsLoaded = true;
				queueTask(loadPreview, (void*)(themes.size()-1), true);
			}
		}
	} else if(
		entry->shortExt[0] == 'Z' &&
		entry->shortExt[1] == 'I' &&
		entry->shortExt[2] == 'P'){
		unzFile zipFile = unzOpen((string("/Themes/") + u16tstr(entry->name, 0x106)).c_str());

		if(!zipFile){
			printf("INVALID ZIP:%s\n", u16tstr(entry->name, 0x106).c_str());
			delete entry;
			return;
		}

		if(unzLocateFile(zipFile, "body_LZ.bin", 0) && unzLocateFile(zipFile, "body_lz.bin", 0)){
			unzClose(zipFile);
			printf("NO BODY:%s\n", u16tstr(entry->name, 0x106).c_str());
			delete entry;
			return;
		}

		Theme theme = {
			u16tstr(entry->name, 0x106),
			u16tstr(entry->name, 0x106),
			ws2s(i18n("no_desc")),
			ws2s(i18n("unknown")),
			NULL,
			NULL,
			true,
			true,
			true,
			false,
			false,
			false
		};

		LightLock_Init(&theme.lock);

		// check if bgm exists
		if(!unzLocateFile(zipFile, "bgm.ogg", 0))
			theme.hasBGM = true;

		// check if preview exists
		if(!unzLocateFile(zipFile, "Preview.png", 0) || !unzLocateFile(zipFile, "preview.png", 0))
			theme.hasPreview = true;

		if(!unzLocateFile(zipFile, "info.smdh", 0)){
			if(!unzOpenCurrentFile(zipFile)){
				vector<char> smdhData;
				if(zippedFileToVector(zipFile, smdhData)){
					unzCloseCurrentFile(zipFile);
				} else {
					if(smdhData.size() == 0x36C0){
						int offset = 0x8;

						string tmpTitle = "";
						string tmpDescription = "";
						string tmpAuthor = "";

						for (size_t i = 0; i < 128; i++){
							if(smdhData[offset + i] == '\00')
								break;

							tmpTitle += smdhData[offset + i];
							offset++;
						}

						offset = 0x8 + 128;

						for (size_t i = 0; i < 256; i++){
							if(smdhData[offset + i] == '\00')
								break;

							tmpDescription += smdhData[offset + i];
							offset++;
						}

						offset = 0x8 + 128 + 256;

						for (size_t i = 0; i < 128; i++){
							if(smdhData[offset + i] == '\00')
								break;

							tmpAuthor += smdhData[offset + i];
							offset++;
						}

						if(tmpTitle.size() != 0)
							theme.title = tmpTitle;

						if(tmpDescription.size() != 0)
							theme.description = tmpDescription;

						if(tmpAuthor.size() != 0 && tmpAuthor.size() < 20)
							theme.author = tmpAuthor;

						// detects default icons
						if(smdhData[0x24C0] != '\x9D' && smdhData[0x24C1] != '\x04' && smdhData[0x24C0] != '\xBF' && smdhData[0x24C1] != '\x0D'){
							theme.icon = sf2d_create_texture(48, 48, TEXFMT_RGB565, SF2D_PLACE_RAM);
							u16* dst = (u16*)(theme.icon->tex.data + 64 * 8 * 2 * sizeof(u16));
							u16* src = (u16*)(&smdhData[0x24C0]);
							for (u8 j = 0; j < 48; j += 8){
								memcpy(dst, src, 48 * 8 * sizeof(u16));
								src += 48 * 8;
								dst += 64 * 8;
							}
						}
					}

					unzCloseCurrentFile(zipFile);
				}
			}
		}

		unzCloseCurrentFile(zipFile);
		unzClose(zipFile);

		themes.push_back(theme);

		if(themes.size() < 3){
			themes[themes.size()-1].previewIsLoaded = true;
			queueTask(loadPreview, (void*)(themes.size()-1), true);
		}
	}

	delete entry;

	themesScanned = true;
}

bool sortDirectoryEntriesByName(FS_DirectoryEntry* a, FS_DirectoryEntry* b){
	string first = u16tstr(a->name, 0x106);
	string second = u16tstr(b->name, 0x106);

	transform(first.begin(), first.end(), first.begin(),(int (*)(int))toupper);
	transform(second.begin(), second.end(), second.begin(),(int (*)(int))toupper);

	return first < second;
}

void scanThemes(void*){
	Result ret;

	Handle themeDir;
	if(FSUSER_OpenDirectory(&themeDir, ARCHIVE_SD, fsMakePath(PATH_ASCII, "/Themes/")))
		return throwError(i18n("err_fail_open", "/Themes/"));

	vector<FS_DirectoryEntry*> entries;

	while (true){
		u32 entriesRead;
		FS_DirectoryEntry* entry = new FS_DirectoryEntry;
		FSDIR_Read(themeDir, &entriesRead, 1, entry);
		if(entriesRead)
			entries.push_back(entry);
		else
			break;
	}

	FSDIR_Close(themeDir);

	sort(entries.begin(), entries.end(), sortDirectoryEntriesByName);

	for (size_t i = 0; i < entries.size(); i++)
		queueTask(loadTheme, (void*)entries[i], false);
}

void loadPreview(void* id){
	if(!themes[(int)id].isZip){
		while (true){
			if(!LightLock_TryLock(&themes[(int)id].lock))
				break;

			svcSleepThread(0x400000LL);
		}

		sf2d_texture* tmp;
		Result res = load_png((string("/Themes/") + themes[(int)id].fileName + "/Preview.png").c_str(), &tmp);

		if(!res){
			themes[(int)id].preview = tmp;
			C3D_TexSetFilter(&themes[(int)id].preview->tex, GPU_LINEAR, GPU_LINEAR);
		} else
			themes[(int)id].hasPreview = false;

		LightLock_Unlock(&themes[(int)id].lock);
	} else {
		// open zip
		while (true){
			if(!LightLock_TryLock(&themes[(int)id].lock))
				break;

			svcSleepThread(0x400000LL);
		}

		unzFile zipFile = unzOpen(string("/Themes/" + string(themes[(int)id].fileName)).c_str());

		if(!zipFile){
			LightLock_Unlock(&themes[(int)id].lock);
			return;
		}

		vector<char> pngData;

		if(unzLocateFile(zipFile, "Preview.png", 0) && unzLocateFile(zipFile, "preview.png", 0)){
			unzClose(zipFile);
			themes[(int)id].hasPreview = false;
			LightLock_Unlock(&themes[(int)id].lock);
			return;
		}

		if(unzOpenCurrentFile(zipFile)){
			unzClose(zipFile);
			themes[(int)id].hasPreview = false;
			LightLock_Unlock(&themes[(int)id].lock);
			return;
		}

		if(zippedFileToVector(zipFile, pngData)){
			unzCloseCurrentFile(zipFile);
			unzClose(zipFile);
			themes[(int)id].hasPreview = false;
			LightLock_Unlock(&themes[(int)id].lock);
			return;
		}

		unzCloseCurrentFile(zipFile);
		unzClose(zipFile);

		sf2d_texture* tmp;
		Result res = load_png_mem(pngData, &tmp);

		if(!res){
			themes[(int)id].preview = tmp;
			C3D_TexSetFilter(&themes[(int)id].preview->tex, GPU_LINEAR, GPU_LINEAR);
		} else
			themes[(int)id].hasPreview = false;

		LightLock_Unlock(&themes[(int)id].lock);
	}
}

void checkInfosToBeLoaded(int id){
	for (int i = 0; i < themes.size(); i++){
		if(i < (id - 4) || i > (id + 4)){
			// just in case the user is scrolling too fast
			while (true){
				if(!LightLock_TryLock(&taskQueueLock))
					break;

				svcSleepThread(0x400000LL);
			}

			for (size_t j = 0; j < taskQueue.size(); j++){
				if(taskQueue[j].arg == (void*)i){
					if(taskQueue[j].entrypoint == loadPreview){
						printf("DELETING TASK FOR THEME %i\n", i);
						themes[i].previewIsLoaded = false;
						taskQueue.erase(taskQueue.begin() + j);
					}
				}
			}

			LightLock_Unlock(&taskQueueLock);

			if(themes[i].hasPreview && themes[i].preview != NULL && !LightLock_TryLock(&themes[i].lock)){
				try {
					themes[i].previewIsLoaded = false;
					sf2d_free_texture(themes[i].preview);
					themes[i].preview = NULL;
				} catch (...){
					LightLock_Unlock(&themes[i].lock);
					printf("error\n");
				}

				LightLock_Unlock(&themes[i].lock);
			}
		} else {
			if(!LightLock_TryLock(&themes[i].lock)){
				if(!themes[i].previewIsLoaded){
					themes[i].previewIsLoaded = true;
					queueTask(loadPreview, (void*)i, true);
				}

				LightLock_Unlock(&themes[i].lock);
			}
		}
	}
}

void installTheme(void* noBGM){
	installProgress = i18n("install_reading", "body_LZ.bin");

	Result ret = 0;
	vector<char> bodyData;
	vector<char> BGMData;

	// Load data
	if(!themes[currentSelectedItem].isZip){
		if(fileToVector(string("/Themes/") + themes[currentSelectedItem].fileName + "/body_LZ.bin", bodyData))
			return throwError(i18n("err_fail_open", "body_LZ.bin"));

		installProgress += wstring(L"\n") + i18n("install_reading", "BGM.bcstm");

		if(!(bool)noBGM)
			fileToVector(string("/Themes/") + themes[currentSelectedItem].fileName + "/bgm.bcstm", BGMData);
	} else {
		unzFile zipFile = unzOpen(string("/Themes/" + string(themes[currentSelectedItem].fileName)).c_str());

		if(!zipFile)
			return throwError(i18n("err_fail_open", "ZIP"));

		if(unzLocateFile(zipFile, "body_LZ.bin", 0) && unzLocateFile(zipFile, "body_lz.bin", 0))
			return throwError(L"Can't find body_LZ.bin file in ZIP");

		if(unzOpenCurrentFile(zipFile))
			return throwError(L"Can't open body_LZ.bin file in ZIP");

		ret = zippedFileToVector(zipFile, bodyData);
		if(ret)
			return throwError(string(string("Can't read body_LZ.bin from ZIP -- error code ") + to_string(ret)).c_str());

		unzCloseCurrentFile(zipFile);

		installProgress += wstring(L"\n") + i18n("install_reading", "BGM.bcstm");

		if(!(bool)noBGM && !unzLocateFile(zipFile, "bgm.bcstm", 0)){
			if(unzOpenCurrentFile(zipFile))
				return throwError(L"Can't open bgm.bcstm file in ZIP");

			ret = zippedFileToVector(zipFile, BGMData);
			if(ret)
				return throwError(string(string("Can't read bgm.bcstm from ZIP -- error code ") + to_string(ret)).c_str());

			unzCloseCurrentFile(zipFile);
		}

		installProgress += wstring(L"\n") + i18n("install_reading", "SaveData.dat");

		unzClose(zipFile);
	}

	// Update SaveData.dat
	u8* saveDataDat_buf;
	u64 saveDataDat_size;
	Handle saveDataDat_handle;

	if(FSUSER_OpenFile(&saveDataDat_handle, ARCHIVE_HomeExt, fsMakePath(PATH_ASCII, "/SaveData.dat"), FS_OPEN_READ | FS_OPEN_WRITE, 0))
		return throwError(i18n("err_fail_open", "SaveData.dat") + wstring(L" ") + i18n("err_try_default"));

	FSFILE_GetSize(saveDataDat_handle, &saveDataDat_size);

	saveDataDat_buf = new u8[saveDataDat_size];
	if(FSFILE_Read(saveDataDat_handle, nullptr, 0, saveDataDat_buf, (u32)saveDataDat_size))
		return throwError(i18n("err_fail_read", "SaveData.dat") + wstring(L" ") + i18n("err_try_default"));

	installProgress += wstring(L"\n") + i18n("install_writing", "SaveData.dat");

	if(
		!(
			saveDataDat_buf[0x141b] == 0 &&
			saveDataDat_buf[0x13b8] != 0 &&
			saveDataDat_buf[0x13bc] == 0 &&
			saveDataDat_buf[0x13bd] == 3
		)
	){
		saveDataDat_buf[0x141b] = 0; // turn off shuffle
		memset(&saveDataDat_buf[0x13b8], 0, 8); // clear the regular theme structure
		saveDataDat_buf[0x13bd] = 3; // make it persistent
		saveDataDat_buf[0x13b8] = 0xff; // theme index

		if(FSFILE_Write(saveDataDat_handle, nullptr, 0, saveDataDat_buf, saveDataDat_size, FS_WRITE_FLUSH))
			return throwError(i18n("err_fail_write", "SaveData.dat") + wstring(L" ") + i18n("err_try_default"));
	}

	free(saveDataDat_buf);
	FSFILE_Close(saveDataDat_handle);

	// Inject body_lz.bin into BodyCache.bin
	Handle bodyCacheBin_handle;

	if(FSUSER_OpenFile(&bodyCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache.bin"), FS_OPEN_WRITE, 0)){
		FSUSER_DeleteFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache.bin"));
		if(FSUSER_CreateFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache.bin"), 0, (u64)0x150000))
			return throwError(L"Failed to create BodyCache.bin." + wstring(L" ") + i18n("err_try_default"));

		if(FSUSER_OpenFile(&bodyCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache.bin"), FS_OPEN_WRITE, 0))
			return throwError(i18n("err_fail_open", "BodyCache.bin") + wstring(L" ") + i18n("err_try_default"));
	}

	installProgress += wstring(L"\n") + i18n("install_writing", "BodyCache.bin");

	if(FSFILE_Write(bodyCacheBin_handle, nullptr, 0, &bodyData[0], (u64)bodyData.size(), FS_WRITE_FLUSH))
		return throwError(i18n("err_fail_write", "BodyCache.bin") + wstring(L" ") + i18n("err_try_default"));

	FSFILE_Close(bodyCacheBin_handle);

	// Inject bgm.bcstm into BgmCache.bin
	Handle bgmCacheBin_handle;

	if(FSUSER_OpenFile(&bgmCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BgmCache.bin"), FS_OPEN_WRITE, 0)){
		FSUSER_DeleteFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BgmCache.bin"));
		if(FSUSER_CreateFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BgmCache.bin"), 0, (u64)3371008))
			return throwError(L"Failed to create BgmCache.bin." + wstring(L" ") + i18n("err_try_default"));

		if(FSUSER_OpenFile(&bgmCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BgmCache.bin"), FS_OPEN_WRITE, 0))
			return throwError(i18n("err_fail_open", "BgmCache.bin") + wstring(L" ") + i18n("err_try_default"));
	}

	installProgress += wstring(L"\n") + i18n("install_writing", "BgmCache.bin");

	if(BGMData.size() != 0)
		ret = FSFILE_Write(bgmCacheBin_handle, nullptr, 0, &BGMData[0], BGMData.size(), FS_WRITE_FLUSH);
	else {
		char* empty = new char[3371008]();
		ret = FSFILE_Write(bgmCacheBin_handle, nullptr, 0, empty, (u64)3371008, FS_WRITE_FLUSH);
		delete[] empty;
	}
	if(ret)
		return throwError(i18n("err_fail_write", "BgmCache.bin") + wstring(L" ") + i18n("err_try_default"));

	FSFILE_Close(bgmCacheBin_handle);

	// Update ThemeManage.bin
	u8* themeManageBin_buf = new u8[0x800];
	Handle themeManageBin_handle;

	//FSUSER_DeleteFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/ThemeManage.bin"));
	//if(FSUSER_CreateFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/ThemeManage.bin"), 0, 0x800))
	//	return throwError(L"Failed to create ThemeManage.bin." + wstring(L" ") + i18n("err_try_default"));

	if(FSUSER_OpenFile(&themeManageBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/ThemeManage.bin"), FS_OPEN_WRITE, 0))
		return throwError(i18n("err_fail_open", "ThemeManage.bin") + wstring(L" ") + i18n("err_try_default"));

	if(FSFILE_Read(themeManageBin_handle, nullptr, 0, themeManageBin_buf, (u32)0x800))
		return throwError(i18n("err_fail_read", "ThemeManage.bin") + wstring(L" ") + i18n("err_try_default"));

	installProgress += wstring(L"\n") + i18n("install_writing", "ThemeManage.bin");

	themeManageBin_buf[0x00] = 1;
	themeManageBin_buf[0x01] = 0;
	themeManageBin_buf[0x02] = 0;
	themeManageBin_buf[0x03] = 0;
	themeManageBin_buf[0x04] = 0;
	themeManageBin_buf[0x05] = 0;
	themeManageBin_buf[0x06] = 0;
	themeManageBin_buf[0x07] = 0;
	u32* bodySize_loc = (u32*)(&themeManageBin_buf[0x08]);
	u32* BGMSize_loc = (u32*)(&themeManageBin_buf[0x0C]);
	*bodySize_loc = (u32)bodyData.size();
	*BGMSize_loc = (u32)BGMData.size();
	themeManageBin_buf[0x10] = 0xFF;
	themeManageBin_buf[0x14] = 0x01;
	themeManageBin_buf[0x18] = 0xFF;
	themeManageBin_buf[0x1D] = 0x02;
	memset(&themeManageBin_buf[0x338], 0, 4);
	memset(&themeManageBin_buf[0x340], 0, 4);
	memset(&themeManageBin_buf[0x360], 0, 4);
	memset(&themeManageBin_buf[0x368], 0, 4);

	//u32 thememanage[0x20>>2];

	//memset(thememanage, 0, 0x20);
	//thememanage[0x0>>2] = 1;
	//thememanage[0x8>>2] = (u32)bodyData.size();
	//thememanage[0xC>>2] = (u32)BGMSize;
	//thememanage[0x10>>2] = 0xff;
	//thememanage[0x14>>2] = 1;
	//thememanage[0x18>>2] = 0xff;
	////thememanage[0x1c>>2] = 0x200;
	//thememanage[0x1D>>2] = 0x02;
	//memset(&thememanage[0x338>>2], 0, 4);
	//memset(&thememanage[0x340>>2], 0, 4);
	//memset(&thememanage[0x360>>2], 0, 4);
	//memset(&thememanage[0x368>>2], 0, 4);

	//memset(themeManageBin_buf, 0, 0x800);
	//memcpy(themeManageBin_buf, thememanage, 0x20);

	if(FSFILE_Write(themeManageBin_handle, nullptr, 0, themeManageBin_buf, 0x800, FS_WRITE_FLUSH))
		return throwError(i18n("err_fail_write", "ThemeManage.bin") + wstring(L" ") + i18n("err_try_default"));

	FSFILE_Close(themeManageBin_handle);

	isInstalling = false;
	installProgress = L"";
}

void installShuffle(void*){
	installProgress = i18n("install_reading", "body_LZ.bin\n& BGM.bcstm");

	Result ret = 0;
	vector<vector<char>> bodyData;
	vector<vector<char>> BGMData;
	int themeCount = 0;

	for (size_t i = 0; i < themes.size(); i++){
		if(themes[i].toShuffle){
			themeCount++;

			// Load data
			if(!themes[i].isZip){
				vector<char> tmpBodyData;
				if(fileToVector(string("/Themes/") + themes[i].fileName + "/body_LZ.bin", tmpBodyData))
					return throwError(L"Failed to open body_LZ.bin file");

				bodyData.push_back(tmpBodyData);
				vector<char> tmpBGMData;

				if(!themes[i].shuffleNoBGM)
					fileToVector(string("/Themes/") + themes[i].fileName + "/bgm.bcstm", tmpBGMData);

				BGMData.push_back(tmpBGMData);
			} else {
				unzFile zipFile = unzOpen(string("/Themes/" + string(themes[i].fileName)).c_str());

				if(!zipFile)
					return throwError(L"Failed to open ZIP file");

				if(unzLocateFile(zipFile, "body_LZ.bin", 0) && unzLocateFile(zipFile, "body_lz.bin", 0))
					return throwError(L"Can't find body_LZ.bin file in ZIP");

				if(unzOpenCurrentFile(zipFile))
					return throwError(L"Can't open body_LZ.bin file in ZIP");

				vector<char> tmpBodyData;
				ret = zippedFileToVector(zipFile, tmpBodyData);
				if(ret)
					return throwError(string(string("Can't read body_LZ.bin from ZIP -- error code ") + to_string(ret)).c_str());

				unzCloseCurrentFile(zipFile);

				bodyData.push_back(tmpBodyData);
				vector<char> tmpBGMData;

				if(!themes[i].shuffleNoBGM && !unzLocateFile(zipFile, "bgm.bcstm", 0)){
					if(unzOpenCurrentFile(zipFile))
						return throwError(L"Can't open bgm.bcstm file in ZIP");

					ret = zippedFileToVector(zipFile, tmpBGMData);
					if(ret)
						return throwError(string(string("Can't read bgm.bcstm from ZIP -- error code ") + to_string(ret)).c_str());

					unzCloseCurrentFile(zipFile);
				}

				BGMData.push_back(tmpBGMData);

				unzClose(zipFile);
			}
		}
	}

	for (size_t i = 0; i < themeCount; i++) {
		printf("%i: %i, %i\n", i, bodyData[i].size(), BGMData[i].size());
	}

	// Update SaveData.dat
	u8* saveDataDat_buf;
	u64 saveDataDat_size;
	Handle saveDataDat_handle;

	if(FSUSER_OpenFile(&saveDataDat_handle, ARCHIVE_HomeExt, fsMakePath(PATH_ASCII, "/SaveData.dat"), FS_OPEN_READ | FS_OPEN_WRITE, 0))
		return throwError(i18n("err_fail_open", "SaveData.dat") + wstring(L" ") + i18n("err_try_default"));

	FSFILE_GetSize(saveDataDat_handle, &saveDataDat_size);

	saveDataDat_buf = new u8[saveDataDat_size];
	if(FSFILE_Read(saveDataDat_handle, nullptr, 0, saveDataDat_buf, (u32)saveDataDat_size))
		return throwError(i18n("err_fail_read", "SaveData.dat") + wstring(L" ") + i18n("err_try_default"));

	installProgress += wstring(L"\n") + i18n("install_writing", "SaveData.dat");

	if(
		!(
			saveDataDat_buf[0x141b] == 1 &&
			saveDataDat_buf[0x13b8] != 0 &&
			saveDataDat_buf[0x13bc] == 0 &&
			saveDataDat_buf[0x13bd] == 3
		)
	){
		saveDataDat_buf[0x141b] = 1; // turn on shuffle
		memset(&saveDataDat_buf[0x13b8], 0, 8); // clear the regular theme structure
		saveDataDat_buf[0x13bd] = 3; // make it persistent
		saveDataDat_buf[0x13b8] = 0xff; // theme index

		for (size_t i = 0; i < 10; i++) {
			memset(&saveDataDat_buf[0x13C0 + 0x8 * i], 0, 8);
			if(themeCount > i){
				saveDataDat_buf[0x13C0 + 0x8 * i] = i;
				saveDataDat_buf[0x13C0 + 0x8 * i + 5] = 3;
			}
		}

		if(FSFILE_Write(saveDataDat_handle, nullptr, 0, saveDataDat_buf, saveDataDat_size, FS_WRITE_FLUSH))
			return throwError(i18n("err_fail_write", "SaveData.dat") + wstring(L" ") + i18n("err_try_default"));
	}

	delete[] saveDataDat_buf;
	FSFILE_Close(saveDataDat_handle);

	// Inject body_lz.bin into BodyCache.bin
	Handle bodyCacheBin_handle;
	if(FSUSER_OpenFile(&bodyCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache_rd.bin"), FS_OPEN_WRITE, 0)){
		FSUSER_DeleteFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache_rd.bin"));
		if(FSUSER_CreateFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache_rd.bin"), 0, 0x150000*10))
			return throwError(L"Failed to create BodyCache_rd.bin." + wstring(L" ") + i18n("err_try_default"));

		if(FSUSER_OpenFile(&bodyCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache_rd.bin"), FS_OPEN_WRITE, 0))
			return throwError(i18n("err_fail_open", "BodyCache_rd.bin") + wstring(L" ") + i18n("err_try_default"));
	}

	installProgress += wstring(L"\n") + i18n("install_writing", "BodyCache_rd.bin");

	for (size_t i = 0; i < 10; i++) {
		if(themeCount > i){
			if(FSFILE_Write(bodyCacheBin_handle, nullptr, 0x150000 * i, &bodyData[i][0], bodyData[i].size(), FS_WRITE_FLUSH))
				return throwError(i18n("err_fail_write", "BodyCache_rd.bin") + wstring(L" ") + i18n("err_try_default"));
		} else {
			char* empty = new char[0x150000]();
			FSFILE_Write(bodyCacheBin_handle, nullptr, 0x150000 * i, empty, 0x150000, FS_WRITE_FLUSH);
			delete[] empty;
		}
	}

	FSFILE_Close(bodyCacheBin_handle);

	installProgress += wstring(L"\n") + i18n("install_writing", "BgmCache_**.bin");

	// Inject bgm.bcstm into BgmCache.bin
	for (size_t i = 0; i < 10; i++){
		Handle bgmCacheBin_handle;
		if(FSUSER_OpenFile(&bgmCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, ("/BgmCache_0" + to_string(i) + ".bin").c_str()), FS_OPEN_WRITE, 0)){
			FSUSER_DeleteFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, ("/BgmCache_0" + to_string(i) + ".bin").c_str()));
			if(FSUSER_CreateFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, ("/BgmCache_0" + to_string(i) + ".bin").c_str()), 0, (u64)3371008))
				return throwError(L"Failed to create BgmCache_0" + to_wstring(i) + L".bin." + wstring(L" ") + i18n("err_try_default"));

			if(FSUSER_OpenFile(&bgmCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, ("/BgmCache_0" + to_string(i) + ".bin").c_str()), FS_OPEN_WRITE, 0))
				return throwError(i18n("err_fail_open", "BgmCache_0" + to_string(i) + ".bin") + wstring(L" ") + i18n("err_try_default"));
		}

		if(themeCount > i && BGMData[i].size() != 0)
			ret = FSFILE_Write(bgmCacheBin_handle, nullptr, 0, &BGMData[i][0], BGMData[i].size(), FS_WRITE_FLUSH);
		else {
			char* empty = new char[3371008]();
			ret = FSFILE_Write(bgmCacheBin_handle, nullptr, 0, empty, (u64)3371008, FS_WRITE_FLUSH);
			delete[] empty;
		}

		if(ret)
			return throwError(i18n("err_fail_write", "BgmCache.bin") + wstring(L" ") + i18n("err_try_default"));

		FSFILE_Close(bgmCacheBin_handle);
	}

	// Update ThemeManage.bin
	u8* themeManageBin_buf = new u8[0x800];
	Handle themeManageBin_handle;

	//FSUSER_DeleteFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/ThemeManage.bin"));
	//if(FSUSER_CreateFile(ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/ThemeManage.bin"), 0, 0x800))
	//	return throwError(L"Failed to create ThemeManage.bin." + wstring(L" ") + i18n("err_try_default"));

	if(FSUSER_OpenFile(&themeManageBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/ThemeManage.bin"), FS_OPEN_WRITE, 0))
		return throwError(i18n("err_fail_open", "ThemeManage.bin") + wstring(L" ") + i18n("err_try_default"));

	if(FSFILE_Read(themeManageBin_handle, nullptr, 0, themeManageBin_buf, (u32)0x800))
		return throwError(i18n("err_fail_read", "ThemeManage.bin") + wstring(L" ") + i18n("err_try_default"));

	installProgress += wstring(L"\n") + i18n("install_writing", "ThemeManage.bin");

	themeManageBin_buf[0x00] = 1;
	themeManageBin_buf[0x01] = 0;
	themeManageBin_buf[0x02] = 0;
	themeManageBin_buf[0x03] = 0;
	themeManageBin_buf[0x04] = 0;
	themeManageBin_buf[0x05] = 0;
	themeManageBin_buf[0x06] = 0;
	themeManageBin_buf[0x07] = 0;
	u32* bodySize_loc = (u32*)(&themeManageBin_buf[0x08]);
	u32* BGMSize_loc = (u32*)(&themeManageBin_buf[0x0C]);
	*bodySize_loc = (u32)0;
	*BGMSize_loc = (u32)0;
	themeManageBin_buf[0x10] = 0xFF;
	themeManageBin_buf[0x14] = 0x01;
	themeManageBin_buf[0x18] = 0xFF;
	themeManageBin_buf[0x1D] = 0x02;
	for (size_t i = 0; i < 10; i++) {
		u32* bodySize_loc = (u32*)(&themeManageBin_buf[0x338 + 4 * i]);
		u32* BGMSize_loc = (u32*)(&themeManageBin_buf[0x360 + 4 * i]);
		if(themeCount > i){
			*bodySize_loc = (u32)bodyData[i].size();
			*BGMSize_loc = (u32)BGMData[i].size();
		} else {
			*bodySize_loc = (u32)0;
			*BGMSize_loc = (u32)0;
		}
	}

	if(FSFILE_Write(themeManageBin_handle, nullptr, 0, themeManageBin_buf, 0x800, FS_WRITE_FLUSH))
		return throwError(i18n("err_fail_write", "ThemeManage.bin") + wstring(L" ") + i18n("err_try_default"));

	FSFILE_Close(themeManageBin_handle);

	exitShuffleMode();

	isInstalling = false;
	installProgress = L"";
}

void exitShuffleMode(){
	for (size_t i = 0; i < themes.size(); i++){
		themes[i].toShuffle = false;
		themes[i].shuffleNoBGM = false;
	}

	shuffleMode = false;
}

void deleteTheme(){
	if(!themes[currentSelectedItem].isZip)
		FSUSER_DeleteDirectoryRecursively(ARCHIVE_SD, fsMakePath(PATH_UTF16, (u"/Themes/" + strtu16str(themes[currentSelectedItem].fileName)).data()));
	else
		FSUSER_DeleteFile(ARCHIVE_SD, fsMakePath(PATH_UTF16, (u"/Themes/" + strtu16str(themes[currentSelectedItem].fileName)).data()));

	themes.erase(themes.begin() + currentSelectedItem);
	selectTheme(max(0, currentSelectedItem - 1));
	deletePrompt = false;
}

void dumpTheme(){
	string num;
	for (size_t i = 0; i < 100; i++){
		char str[3];
		snprintf(str, 3, "%02d", i);
		if(!FSUSER_CreateDirectory(ARCHIVE_SD, fsMakePath(PATH_ASCII, (string("/Themes/Themely_Dump") + str).c_str()), FS_ATTRIBUTE_DIRECTORY)){
			num = string(str);
			break;
		}
	}

	if(num.size() == 0)
		return throwError(L"Ran out of numbers for the dump name. Clean that shit");

	u8* themeManageBin_buf = new u8[0x800];
	Handle themeManageBin_handle;

	u8* bodyCacheBin_buf;
	u64 bodyCacheBin_size;
	Handle bodyCacheBin_handle;

	u8* bgmCacheBin_buf;
	u64 bgmCacheBin_size;
	Handle bgmCacheBin_handle;

	Handle bodyOutput_handle;
	Handle bgmOutput_handle;

	// get size of body and bgm
	if(FSUSER_OpenFile(&themeManageBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/ThemeManage.bin"), FS_OPEN_WRITE, 0))
		return throwError(L"Failed to open ThemeManage.bin");

	if(FSFILE_Read(themeManageBin_handle, nullptr, 0, themeManageBin_buf, (u32)0x800))
		return throwError(L"Failed to read ThemeManage.bin");

	bodyCacheBin_size = (u64)(*((u32*)&themeManageBin_buf[0x08]));
	bgmCacheBin_size = (u64)(*((u32*)&themeManageBin_buf[0x0C]));

	delete[] themeManageBin_buf;
	FSFILE_Close(themeManageBin_handle);

	// get body
	if(FSUSER_OpenFile(&bodyCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BodyCache.bin"), FS_OPEN_READ, 0))
		return throwError(L"Failed to open BodyCache.bin. Perhaps you don't have a theme set?");

	bodyCacheBin_buf = new u8[bodyCacheBin_size];
	if(FSFILE_Read(bodyCacheBin_handle, nullptr, 0, bodyCacheBin_buf, (u32)bodyCacheBin_size))
		return throwError(L"Failed to read BodyCache.bin. Perhaps you don't have a theme set?");

	if(FSUSER_CreateFile(ARCHIVE_SD, fsMakePath(PATH_ASCII, ("/Themes/Themely_Dump" + num + "/body_LZ.bin").c_str()), 0, (u64)bodyCacheBin_size))
		return throwError(L"Failed to create body_LZ.bin");

	if(FSUSER_OpenFile(&bodyOutput_handle, ARCHIVE_SD, fsMakePath(PATH_ASCII, ("/Themes/Themely_Dump" + num + "/body_LZ.bin").c_str()), FS_OPEN_WRITE, 0))
		return throwError(L"Failed to open body_LZ.bin");

	if(FSFILE_Write(bodyOutput_handle, nullptr, 0, bodyCacheBin_buf, bodyCacheBin_size, FS_WRITE_FLUSH))
		return throwError(L"Failed to write to body_LZ.bin");

	delete[] bodyCacheBin_buf;

	// get music
	if(!FSUSER_OpenFile(&bgmCacheBin_handle, ARCHIVE_ThemeExt, fsMakePath(PATH_ASCII, "/BgmCache.bin"), FS_OPEN_READ, 0)){
		bgmCacheBin_buf = new u8[bgmCacheBin_size];
		if(!FSFILE_Read(bgmCacheBin_handle, nullptr, 0, bgmCacheBin_buf, (u32)bgmCacheBin_size)){
			if(!FSUSER_CreateFile(ARCHIVE_SD, fsMakePath(PATH_ASCII, ("/Themes/Themely_Dump" + num + "/bgm.bcstm").c_str()), 0, (u64)bgmCacheBin_size)){
				if(!FSUSER_OpenFile(&bgmOutput_handle, ARCHIVE_SD, fsMakePath(PATH_ASCII, ("/Themes/Themely_Dump" + num + "/bgm.bcstm").c_str()), FS_OPEN_WRITE | FS_OPEN_CREATE, 0)){
					FSFILE_Write(bgmOutput_handle, nullptr, 0, bgmCacheBin_buf, bgmCacheBin_size, FS_WRITE_FLUSH);

					FSFILE_Close(bgmOutput_handle);
				}
			}
		}

		delete[] bgmCacheBin_buf;
		FSFILE_Close(bgmCacheBin_handle);
	}

	FSFILE_Close(bodyCacheBin_handle);
	FSFILE_Close(bodyOutput_handle);

	dumpPrompt = false;
}

void toggleBGM(){
	if(!themes[currentSelectedItem].hasBGM)
		return;

	if(!fileExists(u"/3ds/dspfirm.cdc"))
		return throwError(i18n("err_no_dsp"));

	string path;

	if(themes[currentSelectedItem].isZip){
		unzFile zipFile = unzOpen(string("/Themes/" + string(themes[currentSelectedItem].fileName)).c_str());

		if(!zipFile)
			return throwError(L"Failed to open ZIP file");

		if(unzLocateFile(zipFile, "bgm.ogg", 0))
			return throwError(L"Can't find bgm.ogg file in ZIP");

		if(unzOpenCurrentFile(zipFile))
			return throwError(L"Can't open bgm.ogg file in ZIP");

		std::vector<char> oggData;
		int ret = zippedFileToVector(zipFile, oggData);
		if(ret)
			return throwError(string(string("Can't read bgm.ogg from ZIP -- error code ") + to_string(ret)).c_str());

		unzCloseCurrentFile(zipFile);
		unzClose(zipFile);

		remove("/3ds/Themely/tmp/bgm.ogg");
		ofstream tmpOggFile("/3ds/Themely/tmp/bgm.ogg", ios::out | ios::binary);
		tmpOggFile.write(&oggData[0], oggData.size());
		tmpOggFile.close();

		path = "/3ds/Themely/tmp/bgm.ogg";
	} else
		path = string("/Themes/") + themes[currentSelectedItem].fileName + "/bgm.ogg";

	if(!audioIsPlaying && currentPlayingAudio == NULL){
		audioIsPlaying = true;
		AUDIO_load(path);
	} else
		AUDIO_stop();
}
