import os
import dotenv
import time
import datetime
dotenv_file = dotenv.find_dotenv('SOFTWARE_VERSION')
dotenv.load_dotenv(dotenv_file)

VERSION_MAJOR = int(os.environ['VERSION_MAJOR'])
VERSION_BOARD_TYPE = int(os.environ['VERSION_BOARD_TYPE'])
VERSION_HW = int(os.environ['VERSION_HW'])
VERSION_MINOR = int(os.environ['VERSION_MINOR'])
if (VERSION_MINOR + 1) > 255:
        VERSION_MINOR = 0
VERSION_ = (VERSION_MAJOR<<24)|(VERSION_BOARD_TYPE<<16)|(VERSION_HW<<8)|(VERSION_MINOR+1)
print(VERSION_)
os.environ['VERSION_MINOR'] = str(VERSION_MINOR +1)
os.environ['BUILD_DATE']=datetime.datetime.now().strftime("%Y%m%d")
os.environ['BUILD_TIMESTAMP']=str(int(time.time()))
os.environ['VERSION_']=str(VERSION_)

dotenv.set_key(dotenv_file, "VERSION_MINOR", os.environ["VERSION_MINOR"])
dotenv.set_key(dotenv_file, "BUILD_DATE", os.environ["BUILD_DATE"])
dotenv.set_key(dotenv_file, "BUILD_TIMESTAMP", os.environ["BUILD_TIMESTAMP"])
dotenv.set_key(dotenv_file, "VERSION_", os.environ["VERSION_"])
