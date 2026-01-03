##
#   Native RPC Common
#
#       CONFIG_NAME
#       COMMON_TYPES
#       SchemaInfo
#       FieldInfo
#       MethodInfo
#       Options
#       Connection
#       Service
#
#       verifyPython
#       getProjectName
#       getProjectPath
#       getEntryPoint
#       getMessageFiles
#       parseSchemaList
#       getShellId
##
import __main__
import os
from . import parser
import psutil
import subprocess
import sys
from typing import TypedDict, Final

CONFIG_NAME = "workspace.json"

COMMON_TYPES = {
    "dict": dict,
    "str": str,
    "bool": bool,
    "int": int,
    "float": float,
    "list": list,
}


class SchemaInfo:
    className: str
    fieldName: str
    fieldType: str
    methodName: str
    methodRequest: str
    methodResponse: str
    idNumber: int

    def __init__(self, **kwargs):
        self.className = kwargs["className"]
        self.fieldName = kwargs.get("fieldName")
        self.fieldType = kwargs.get("fieldType")
        self.methodName = kwargs.get("methodName")
        self.methodRequest = kwargs.get("methodRequest")
        self.methodResponse = kwargs.get("methodResponse")
        self.idNumber = kwargs["idNumber"]
        assert isinstance(self.className, str)


class FieldInfo:
    className: str
    classType: type
    fieldName: str
    fieldType: type
    idNumber: int

    def __init__(self, **kwargs):
        self.className = kwargs["className"]
        self.classType = kwargs["classType"]
        self.fieldName = kwargs["fieldName"]
        self.fieldType = kwargs["fieldType"]
        self.idNumber = kwargs["idNumber"]


class MethodInfo:
    className: str
    classType: type
    methodName: str
    methodCall: any
    methodParams: list
    idNumber: int

    def __init__(self, **kwargs):
        self.className = kwargs["className"]
        self.classType = kwargs["classType"]
        self.methodName = kwargs["methodName"]
        self.methodCall = kwargs["methodCall"]
        self.methodParams = kwargs["methodParams"]
        self.idNumber = kwargs["idNumber"]


SERVICE: Final = "service"
HOST: Final = "host"

class Options(TypedDict):
    service: type
    host: tuple[str, int]



class Connection:
    connectionId: int
    socket: any
    address: tuple
    stime: float
    wtime: float
    readBuffer: bytes
    closed: bool
    messageCount: int
    senderId: str
    callId: str
    processId: str
    parentId: str
    shellId: str
    projectId: str
    entryPoint: str

    def __init__(self, **kwargs):
        self.connectionId = kwargs["connectionId"]
        self.socket = kwargs["socket"]
        self.address = kwargs["address"]
        self.stime = kwargs["stime"]
        self.wtime = kwargs["wtime"]
        self.projectId = kwargs["projectId"]
        self.readBuffer = kwargs.get("readBuffer", b"")
        self.closed = kwargs.get("closed", False)
        self.messageCount = kwargs.get("messageCount", 0)
        self.senderId = kwargs.get("senderId", "")
        self.callId = kwargs.get("callId", "")
        self.processId = kwargs.get("processId", "")
        self.parentId = kwargs.get("parentId", "")
        self.shellId = kwargs.get("shellId", "")
        self.entryPoint = kwargs.get("entryPoint", "")


class Service:
    client: any

    def __init__(self, client):
        self.client = client

    def close(self):
        self.client.close()


def verifyPython():
    res = subprocess.check_output(["python", "--version"]).decode()
    res = res.replace("Python ", "").strip()
    if f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}" != res:
        raise RuntimeError(f"Mismatching python version: {sys.version_info}, {res}")


def getProjectName():
    if not hasattr(__main__, '__file__'):
        return "unknown"
    folder = os.path.dirname(__main__.__file__)
    while True:
        if os.path.exists(os.path.join(folder, 'package.json')) or \
                os.path.exists(os.path.join(folder, 'pyproject.toml')) or \
                os.path.exists(os.path.join(folder, 'tsconfig.json')) or \
                os.path.exists(os.path.join(folder, '.git')):
            break
        if os.path.exists(os.path.join(os.path.dirname(folder), 'workspace.json')):
            break
        folder = os.path.join(folder, '..')
        folder = os.path.abspath(folder)
    return os.path.basename(folder)


def getProjectPath():
    if not hasattr(__main__, '__file__'):
        return "unknown"
    folder = os.path.dirname(__main__.__file__)
    while True:
        if os.path.exists(os.path.join(folder, 'package.json')) or \
                os.path.exists(os.path.join(folder, 'pyproject.toml')) or \
                os.path.exists(os.path.join(folder, 'tsconfig.json')) or \
                os.path.exists(os.path.join(folder, '.git')):
            break
        if os.path.exists(os.path.join(os.path.dirname(folder), 'workspace.json')):
            break
        folder = os.path.join(folder, '..')
        folder = os.path.abspath(folder)
    return folder


def getEntryPoint():
    return __main__.__file__


def getMessageFiles(projectPath):
    # TODO
    #   settings = None
    #   with open(os.path.join(package_dir, 'workspace.json'), 'rt') as file:
    #       settings = json.load(file)
    #   for schemaName in settings.get('schemaNames', ['common']):
    #       full = os.path.join(getProjectPath(), "src", schemaName + ".py")
    schemaName = "common"
    result = [os.path.join(projectPath, "src", schemaName + ".py")]
    assert os.path.exists(result[0])
    return result


def parseSchemaList(file):
    return parser.parseSchemaList(file)


def getShellId():
    return ":".join([f"{x.pid}" for x in psutil.Process(os.getppid()).parent().parents()])
