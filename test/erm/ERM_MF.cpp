/*
 * ERM_MF.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "../scripting/ScriptFixture.h"
#include "../JsonComparer.h"

#include "../../lib/NetPacks.h"

namespace battle
{
namespace events
{

}
}

namespace test
{
namespace scripting
{

using namespace ::testing;

class ERM_MF : public Test, public ScriptFixture
{
protected:
	void SetUp() override
	{
		ScriptFixture::setUp();
	}
};

TEST_F(ERM_MF, ChangeDamage)
{
	std::stringstream source;
	source << "VERM" << std::endl;
	source << "!?MF1;" << std::endl;
	source << "!!MF:D?y-1;" << std::endl;
	source << "!!VRy-1:+10;" << std::endl;
	source << "!!MF:Fy-1;" << std::endl;

	loadScript(VLC->scriptHandler->erm, source.str());
	SCOPED_TRACE("\n" + subject->code);
	run();

}

}
}

