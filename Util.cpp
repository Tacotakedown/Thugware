#include <string>

std::string getNameFromID(int ID) {
	try {
		DWORD_PTR fNamePtr = MemoryM.read<DWORD_PTR>(GName_PTR + int(ID / 0x4000) * 0x8);
		DWORD_PTR fName = MemoryM.read<DWORD_PTR>(fNamePtr + 0x8 * int(ID % 0x4000));
		return MemoryM.readStringUnformated(fName + 0x10, 256);
	}
	catch (int e) { return string(""); }

}