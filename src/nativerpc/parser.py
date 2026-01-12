##
#   Native RPC Code Parser
#
#       Color
#       NodeType
#
#       TreeNode
#           __init__
#           getText
#           getShortType
#           children
#           getSimple
#           getParams
#           getFormatted
#
#       PythonParser
#           __init__
#           parseTree
#           addNodeDict
#           addNodeWord
#           getBlockIndent
#
#       TypescriptParser
#           __init__
#           parseTree
#           addNodeDict
#           addNodeWord
#
#       CppParser
#           __init__
#           parseTree
#           addNodeDict
#           addNodeWord
#
#       parseSchemaScene
#       parseSchemaList
#       __main__
##
import sys
import json
import os
from enum import Enum
from typing import Self


class Color:
    BLACK = "\033[30m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    RESET = "\033[39m"
    BRIGHT = "\033[1m"
    DIM = "\033[2m"
    NORMAL = "\033[22m"
    RESET_ALL = "\033[0m"


class NodeType(Enum):
    ROOT = 1
    # Last unclassified child
    ENDSTATEMENT = 2
    CLASS = 3
    CALL = 4
    FETCH = 5
    CURLY = 6
    WIGGLY = 7
    STATEMENT = 8
    SQUARE = 9
    ARGUMENT = 10
    WORD = 11
    OPERATOR = 12
    STRING = 13
    COMMENT = 14
    BLOCK = 15


class TreeNode:
    nodeType: NodeType
    parentNode: Self
    textData: str
    childList: list[Self]
    startIndex: int
    endIndex: int
    blockIndent: int

    def __init__(
        self,
        parentNode: Self,
        nodeType: NodeType,
        textData: str,
        children: list,
        startIndex: int,
        endIndex: int,
        blockIndent: int
    ):
        assert parentNode is None or isinstance(parentNode, TreeNode)
        self.nodeType = nodeType
        self.parentNode = parentNode
        self.textData = textData
        self.childList = children[:] if children else []
        self.startIndex = startIndex
        self.endIndex = endIndex
        self.blockIndent = blockIndent

    def getText(self):
        result = \
            'root' if self.nodeType == NodeType.ROOT else \
            'statement' if self.nodeType == NodeType.ENDSTATEMENT else \
            'argument' if self.nodeType == NodeType.ARGUMENT else \
            'comment' if self.nodeType == NodeType.COMMENT else \
            self.textData if self.nodeType == NodeType.STRING else \
            'class' if self.nodeType == NodeType.CLASS else \
            'call' if self.nodeType == NodeType.CALL else \
            'statement' if self.nodeType == NodeType.STATEMENT else \
            'wiggly' if self.nodeType == NodeType.BLOCK else \
            'wiggly' if self.nodeType == NodeType.WIGGLY else \
            'curly' if self.nodeType == NodeType.CURLY else \
            'square' if self.nodeType == NodeType.SQUARE else \
            self.textData if self.textData is not None else \
            ''
        return result

    def getShortType(self):
        return \
            'rtn' if self.nodeType == NodeType.ROOT else \
            'est' if self.nodeType == NodeType.ENDSTATEMENT else \
            'cmt' if self.nodeType == NodeType.COMMENT else \
            'arg' if self.nodeType == NodeType.ARGUMENT else \
            'opr' if self.nodeType == NodeType.OPERATOR else \
            'crl' if self.nodeType == NodeType.CURLY else \
            'sqr' if self.nodeType == NodeType.SQUARE else \
            'lvl' if self.nodeType == NodeType.BLOCK else \
            'wgl' if self.nodeType == NodeType.WIGGLY else \
            'str' if self.nodeType == NodeType.STRING else \
            'stm' if self.nodeType == NodeType.STATEMENT else \
            'wrd' if self.nodeType == NodeType.WORD else \
            'cll' if self.nodeType == NodeType.CALL else \
            'cls' if self.nodeType == NodeType.CLASS else \
            self.nodeType.name

    @property
    def children(self) -> list[Self]:
        return self.childList

    def getSimple(self, mask) -> list[str]:
        result = [self.getText()]
        for index in range(0, len(self.childList)):
            if (mask & (1 << index)) != 0:
                result.append('*')
            else:
                result.append(self.childList[index].getText())
        return result

    def getParams(self, parts) -> list[str]:
        if 1 + len(self.childList) != len(parts):
            return None
        parts2 = self.getSimple(0)
        for index in range(0, len(parts)):
            if parts[index] != "*" and parts[index] != parts2[index]:
                return None
        result = []
        for index in range(0, len(parts)):
            if parts[index] == "*":
                result.append(parts2[index])
        return result

    def getFormatted(self, indent: str, parent_path: str = ''):
        result = []
        text = self.getText()
        if not text:
            text = "-"
        colorValue = \
            Color.RED if self.nodeType == NodeType.ROOT else \
            Color.RED if self.nodeType == NodeType.ENDSTATEMENT else \
            Color.DIM if self.nodeType == NodeType.COMMENT else \
            Color.MAGENTA if self.nodeType == NodeType.CLASS else \
            Color.BLUE if self.nodeType == NodeType.CALL else \
            Color.BLUE if self.nodeType == NodeType.STATEMENT else \
            Color.BLUE if self.nodeType == NodeType.ARGUMENT else \
            Color.GREEN if self.nodeType == NodeType.WORD else \
            Color.CYAN if self.nodeType == NodeType.OPERATOR else \
            Color.YELLOW
        name = self.getShortType()

        result.append(f'{indent}{colorValue}{text}{Color.RESET}{Color.RESET_ALL}')

        assert text, f'Unknown text: {name}, {self.nodeType}, "{self.textData}", {parent_path}.{text}'
        for child in self.childList:
            result.extend(child.getFormatted(indent + '    ', f'{parent_path}.{text}'))

        return result


class PythonParser:
    textData: str
    lineIndex: int
    pathIndex: int
    pathList: list[TreeNode]
    nodeList: list[list[TreeNode]]
    wordType: int
    currentWord: str

    def __init__(self):
        self.textData = None
        self.lineIndex = 0
        self.pathIndex = 0
        self.pathList = None
        self.nodeList = None
        self.wordType = 0
        self.currentWord = None
        self.scene = None

    def parseTree(self, data):
        self.textData = data
        self.lineIndex = 0
        self.pathIndex = 0
        self.pathList = [TreeNode(
            parentNode=None,
            nodeType=NodeType.ROOT,
            textData=None,
            children=None,
            startIndex=0,
            endIndex=len(self.textData),
            blockIndent=0
        )]
        self.nodeList = [[]]
        self.wordType = 0
        self.currentWord = None
        self.is_done = False

        while self.lineIndex < len(self.textData) and not self.is_done:
            char = self.textData[self.lineIndex]
            two_char = self.textData[self.lineIndex:self.lineIndex+2]
            small = ord(char) >= ord('a') and ord(char) <= ord('z')
            big = ord(char) >= ord('A') and ord(char) <= ord('Z')
            digit = ord(char) >= ord('0') and ord(char) <= ord('9')
            is_special = char in ['.', '_', '#']
            in_scope = self.pathList[self.pathIndex].nodeType == NodeType.WIGGLY or self.pathList[
                self.pathIndex].nodeType == NodeType.CURLY or self.pathList[self.pathIndex].nodeType == NodeType.SQUARE
            is_command_end = not in_scope and char == '\n'
            is_argument_end = in_scope and char == ','
            command_end_next_indent = self.getBlockIndent(self.lineIndex + 1) if is_command_end else None
            is_scope_end = False
            if is_command_end and command_end_next_indent < self.pathList[self.pathIndex].blockIndent:
                is_scope_end = True
            start_type = \
                NodeType.BLOCK if two_char == ':\n' else \
                NodeType.WIGGLY if char == '{' else \
                NodeType.CURLY if char == '(' else \
                NodeType.SQUARE if char == '[' else \
                None
            scope_end_char = \
                '}' if self.pathList[self.pathIndex].nodeType == NodeType.WIGGLY else \
                ')' if self.pathList[self.pathIndex].nodeType == NodeType.CURLY else \
                ']' if self.pathList[self.pathIndex].nodeType == NodeType.SQUARE else \
                None
            end_type = self.pathList[self.pathIndex].nodeType if scope_end_char is not None and char == scope_end_char else None

            # Strings
            if two_char in ['f"', "f'"]:
                if self.currentWord is not None:
                    self.addNodeWord()
                end_chr = two_char[1:]
                endIndex = self.textData.find(end_chr, self.lineIndex + 2)
                assert endIndex != -1
                value = self.textData[self.lineIndex+2:endIndex]
                self.nodeList[self.pathIndex].append(TreeNode(
                    parentNode=self.pathList[self.pathIndex],
                    nodeType=NodeType.STRING,
                    textData=value,
                    children=None,
                    startIndex=self.lineIndex,
                    endIndex=endIndex,
                    blockIndent=self.pathList[self.pathIndex].blockIndent
                ))
                self.lineIndex = endIndex + 1
                continue
            elif char in ['"', "'"]:
                if self.currentWord is not None:
                    self.addNodeWord()
                end_chr = char
                endIndex = self.textData.find(end_chr, self.lineIndex + 1)
                assert endIndex != -1
                value = self.textData[self.lineIndex+1:endIndex]
                self.nodeList[self.pathIndex].append(TreeNode(
                    parentNode=self.pathList[self.pathIndex],
                    nodeType=NodeType.STRING,
                    textData=value,
                    children=None,
                    startIndex=self.lineIndex,
                    endIndex=endIndex,
                    blockIndent=self.pathList[self.pathIndex].blockIndent
                ))
                self.lineIndex = endIndex + 1
                continue

            # Comments
            if char == '#':
                if self.currentWord is not None:
                    self.addNodeWord()
                endIndex = self.textData.find('\n', self.lineIndex)
                if endIndex == -1:
                    self.is_done = True
                    continue
                comment = self.textData[self.lineIndex:endIndex]
                if len(self.nodeList[self.pathIndex]) > 0:
                    self.nodeList[self.pathIndex].append(TreeNode(
                        parentNode=self.pathList[self.pathIndex],
                        nodeType=NodeType.COMMENT,
                        textData=comment,
                        children=None,
                        startIndex=self.lineIndex,
                        endIndex=endIndex,
                        blockIndent=self.pathList[self.pathIndex].blockIndent,
                    ))
                else:
                    self.pathList[self.pathIndex].childList.append(TreeNode(
                        parentNode=self.pathList[self.pathIndex],
                        nodeType=NodeType.COMMENT,
                        textData=comment,
                        children=None,
                        startIndex=self.lineIndex,
                        endIndex=endIndex,
                        blockIndent=self.pathList[self.pathIndex].blockIndent,
                    ))
                self.lineIndex = endIndex + 1
                continue

            # Newline whitespace
            elif two_char == '\\\n':
                # Start word
                if self.wordType != 1:
                    if self.currentWord is not None:
                        self.addNodeWord()
                    self.wordType = 1

                # Collect words
                self.currentWord = ' ' if self.currentWord is None else self.currentWord + ' '
                self.lineIndex += 2
                continue

            # Scope starts and ends
            elif start_type is not None:
                if self.currentWord is not None:
                    self.addNodeWord()
                new_child = TreeNode(
                    parentNode=self.pathList[self.pathIndex],
                    nodeType=start_type,
                    textData=None,
                    children=None,
                    startIndex=self.lineIndex,
                    endIndex=None,
                    blockIndent=self.getBlockIndent(
                        self.lineIndex + 2) if start_type == NodeType.BLOCK else self.pathList[self.pathIndex].blockIndent
                )
                self.nodeList[self.pathIndex].append(new_child)
                self.pathIndex = len(self.pathList)
                self.pathList = [*self.pathList, new_child]
                self.nodeList = [*self.nodeList, []]
                self.lineIndex += 2 if start_type == NodeType.BLOCK else 1
                continue

            elif end_type is not None:
                assert len(self.pathList) > 1
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.pathList[self.pathIndex].endIndex = self.lineIndex
                self.pathIndex = len(self.pathList) - 2
                self.pathList = self.pathList[0:-1]
                self.nodeList = self.nodeList[0:-1]
                self.lineIndex += 1
                if end_type == NodeType.WIGGLY:
                    statement_type = \
                        NodeType.CLASS if end_type == NodeType.WIGGLY else \
                        NodeType.CALL if end_type == NodeType.CURLY else \
                        None
                    if self.currentWord is not None:
                        self.addNodeWord()
                    if self.nodeList[self.pathIndex]:
                        self.addNodeDict(statement_type)
                continue

            elif is_scope_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.lineIndex += 1

                while True:
                    assert len(self.pathList) > 1
                    self.pathList[self.pathIndex].endIndex = self.lineIndex
                    self.pathIndex = len(self.pathList) - 2
                    self.pathList = self.pathList[0:-1]
                    self.nodeList = self.nodeList[0:-1]
                    if self.pathList[self.pathIndex].blockIndent <= command_end_next_indent:
                        break
                    in_scope = self.pathList[self.pathIndex].nodeType == NodeType.WIGGLY or self.pathList[
                        self.pathIndex].nodeType == NodeType.CURLY or self.pathList[self.pathIndex].nodeType == NodeType.SQUARE
                    if self.currentWord is not None:
                        self.addNodeWord()
                    if self.nodeList[self.pathIndex]:
                        self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)

                continue

            # Statement and argument end
            elif is_command_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.lineIndex += 1
                continue

            # End of statements
            elif is_argument_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT)
                self.lineIndex += 1
                continue

            # Start word
            wordType = 1 if char in [' ', '\r', '\n', '\t'] else 2 if small or big or digit or is_special else 3
            if self.wordType != wordType:
                if self.currentWord is not None:
                    self.addNodeWord()
                self.wordType = wordType

            # Collect words
            self.currentWord = char if self.currentWord is None else self.currentWord + char
            self.lineIndex += 1

        assert self.pathIndex == len(self.pathList) - 1
        while len(self.pathList) > 1:
            if self.currentWord is not None:
                self.addNodeWord()
            if self.nodeList[self.pathIndex]:
                self.addNodeDict(NodeType.ENDSTATEMENT)
            assert len(self.pathList) > 1
            self.pathIndex = len(self.pathList) - 2
            self.pathList = self.pathList[0:-1]
            self.nodeList = self.nodeList[0:-1]

        assert self.pathIndex == len(self.pathList) - 1
        assert len(self.pathList) == 1
        if self.currentWord is not None:
            self.addNodeWord()
        if self.nodeList[self.pathIndex]:
            self.addNodeDict(NodeType.ENDSTATEMENT)
        assert len(self.pathList) == 1
        assert len(self.nodeList) == 1
        assert len(self.nodeList[0]) == 0
        self.scene = self.pathList[0]

    def addNodeDict(self, nodeType):
        assert self.pathIndex == len(self.pathList) - 1
        assert self.currentWord is None
        assert nodeType is not None
        assert self.nodeList[self.pathIndex]
        startIndex = self.nodeList[self.pathIndex][0].startIndex
        if self.nodeList[self.pathIndex][len(self.nodeList[self.pathIndex]) - 1].endIndex is None:
            self.nodeList[self.pathIndex][len(self.nodeList[self.pathIndex]) - 1].endIndex = self.lineIndex
        new_child = TreeNode(
            parentNode=self.pathList[self.pathIndex],
            nodeType=nodeType,
            textData=None,
            children=self.nodeList[self.pathIndex],
            startIndex=startIndex,
            endIndex=self.lineIndex,
            blockIndent=self.pathList[self.pathIndex].blockIndent
        )
        for item in new_child.childList:
            item.parentNode = item
        self.pathList[self.pathIndex].childList.append(new_child)
        self.nodeList[self.pathIndex] = []

    def addNodeWord(self):
        assert self.pathIndex == len(self.pathList) - 1
        assert self.currentWord is not None

        if self.wordType == 2 or self.wordType == 3:
            self.nodeList[self.pathIndex].append(TreeNode(
                parentNode=None,
                nodeType=NodeType.OPERATOR if self.currentWord in [
                    "if", "for", "while", "def", "pass", "class", "str", "int", "bool", "and", "or"] else NodeType.WORD if self.wordType == 2 else NodeType.OPERATOR,
                textData=self.currentWord,
                children=None,
                startIndex=self.lineIndex-len(self.currentWord),
                endIndex=self.lineIndex,
                blockIndent=self.pathList[self.pathIndex].blockIndent
            ))

        self.wordType = 0
        self.currentWord = None

    def getBlockIndent(self, lineIndex):
        stepped = 0
        total = 0
        while lineIndex + stepped < len(self.textData):
            if self.textData[lineIndex + stepped] == '#':
                while lineIndex + stepped < len(self.textData):
                    if self.textData[lineIndex + stepped] == '\n':
                        stepped += 1
                        break
                    stepped += 1
                total = 0
                continue

            elif self.textData[lineIndex + stepped] == '\n':
                stepped += 1
                total = 0
                continue

            elif self.textData[lineIndex + stepped] == '\t':
                total += 1
                stepped += 1
                continue

            elif self.textData[lineIndex + stepped] == ' ':
                total += 1
                stepped += 1
                continue

            break

        if lineIndex + stepped < len(self.textData):
            return total

        return 0


class TypescriptParser:
    textData: str
    lineIndex: int
    pathIndex: int
    pathList: list[TreeNode]
    nodeList: list[list[TreeNode]]
    wordType: int
    currentWord: str

    def __init__(self):
        self.textData = None
        self.lineIndex = 0
        self.pathIndex = 0
        self.pathList = None
        self.nodeList = None
        self.wordType = 0
        self.currentWord = None
        self.scene = None

    def parseTree(self, data):
        self.textData = data
        self.lineIndex = 0
        self.pathIndex = 0
        self.pathList = [TreeNode(
            parentNode=None,
            nodeType=NodeType.ROOT,
            textData=None,
            children=None,
            startIndex=0,
            endIndex=len(self.textData),
            blockIndent=0
        )]
        self.nodeList = [[]]
        self.wordType = 0
        self.currentWord = None
        self.is_done = False

        while self.lineIndex < len(self.textData) and not self.is_done:
            char = self.textData[self.lineIndex]
            two_char = self.textData[self.lineIndex:self.lineIndex+2]
            small = ord(char) >= ord('a') and ord(char) <= ord('z')
            big = ord(char) >= ord('A') and ord(char) <= ord('Z')
            digit = ord(char) >= ord('0') and ord(char) <= ord('9')
            is_special = char in ['.', '_', '#']
            in_scope2 = self.pathList[self.pathIndex].nodeType == NodeType.WIGGLY or self.pathList[
                self.pathIndex].nodeType == NodeType.CURLY or self.pathList[self.pathIndex].nodeType == NodeType.SQUARE
            in_scope = self.pathList[
                self.pathIndex].nodeType == NodeType.CURLY or self.pathList[self.pathIndex].nodeType == NodeType.SQUARE
            is_command_end = char == ';'
            is_argument_end = in_scope2 and char == ','
            start_type = \
                NodeType.WIGGLY if char == '{' else \
                NodeType.CURLY if char == '(' else \
                NodeType.SQUARE if char == '[' else \
                None
            scope_end_char = \
                '}' if self.pathList[self.pathIndex].nodeType == NodeType.WIGGLY else \
                ')' if self.pathList[self.pathIndex].nodeType == NodeType.CURLY else \
                ']' if self.pathList[self.pathIndex].nodeType == NodeType.SQUARE else \
                None
            end_type = self.pathList[self.pathIndex].nodeType if scope_end_char is not None and char == scope_end_char else None
            is_include_end = \
                len(self.nodeList[self.pathIndex]) > 0 and \
                self.nodeList[self.pathIndex][0].nodeType == NodeType.WORD and \
                self.nodeList[self.pathIndex][0].textData == 'import' and \
                len(self.nodeList[self.pathIndex]) == 4 and \
                char == '\n'
            is_field_end = \
                len(self.nodeList[self.pathIndex]) >= 2 and \
                self.nodeList[self.pathIndex][1].getText() == ":" and \
                char == '\n'

            # Include end
            if is_include_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.lineIndex += 1
                continue

            # Strings
            if char in ['"', "'", "`"]:
                if self.currentWord is not None:
                    self.addNodeWord()
                end_chr = char
                endIndex = self.textData.find(end_chr, self.lineIndex + 1)
                assert endIndex != -1
                value = self.textData[self.lineIndex+1:endIndex]
                self.nodeList[self.pathIndex].append(TreeNode(
                    parentNode=self.pathList[self.pathIndex],
                    nodeType=NodeType.STRING,
                    textData=value,
                    children=None,
                    startIndex=self.lineIndex,
                    endIndex=endIndex,
                    blockIndent=self.pathList[self.pathIndex].blockIndent
                ))
                self.lineIndex = endIndex + 1
                continue

            # Comments
            if two_char in ['//', '/*']:
                if self.currentWord is not None:
                    self.addNodeWord()
                endIndex = self.textData.find('\n' if two_char == '//' else '*/', self.lineIndex)
                if endIndex == -1:
                    self.is_done = True
                    continue
                comment = self.textData[self.lineIndex:endIndex]
                if len(self.nodeList[self.pathIndex]) > 0:
                    self.nodeList[self.pathIndex].append(TreeNode(
                        parentNode=self.pathList[self.pathIndex],
                        nodeType=NodeType.COMMENT,
                        textData=comment,
                        children=None,
                        startIndex=self.lineIndex,
                        endIndex=endIndex,
                        blockIndent=self.pathList[self.pathIndex].blockIndent,
                    ))
                else:
                    self.pathList[self.pathIndex].childList.append(TreeNode(
                        parentNode=self.pathList[self.pathIndex],
                        nodeType=NodeType.COMMENT,
                        textData=comment,
                        children=None,
                        startIndex=self.lineIndex,
                        endIndex=endIndex,
                        blockIndent=self.pathList[self.pathIndex].blockIndent,
                    ))
                self.lineIndex = endIndex + 2
                continue

            # Scope starts and ends
            if start_type is not None:
                if self.currentWord is not None:
                    self.addNodeWord()
                new_child = TreeNode(
                    parentNode=self.pathList[self.pathIndex],
                    nodeType=start_type,
                    textData=None,
                    children=None,
                    startIndex=self.lineIndex,
                    endIndex=None,
                    blockIndent=0,
                )
                self.nodeList[self.pathIndex].append(new_child)
                self.pathIndex = len(self.pathList)
                self.pathList = [*self.pathList, new_child]
                self.nodeList = [*self.nodeList, []]
                self.lineIndex += 2 if start_type == NodeType.BLOCK else 1
                continue

            elif end_type is not None:
                assert len(self.pathList) > 1
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.pathList[self.pathIndex].endIndex = self.lineIndex
                self.pathIndex = len(self.pathList) - 2
                self.pathList = self.pathList[0:-1]
                self.nodeList = self.nodeList[0:-1]
                self.lineIndex += 1
                # includes are 4-item long statements
                if end_type == NodeType.WIGGLY and len(self.nodeList[self.pathIndex]) == 2 and \
                        self.nodeList[self.pathIndex][0].getText() == "import":
                    pass
                # class/function-body/dictionary-definition should be a statement
                elif end_type == NodeType.WIGGLY:
                    statement_type = \
                        NodeType.STATEMENT if end_type == NodeType.WIGGLY else \
                        NodeType.CALL if end_type == NodeType.CURLY else \
                        None
                    if self.currentWord is not None:
                        self.addNodeWord()
                    if self.nodeList[self.pathIndex]:
                        self.addNodeDict(statement_type)
                # class annotation should be a statement
                elif end_type == NodeType.CURLY and len(self.nodeList[self.pathIndex]) > 0 and self.nodeList[self.pathIndex][0].getText() == "@":
                    statement_type = NodeType.STATEMENT
                    if self.currentWord is not None:
                        self.addNodeWord()
                    if self.nodeList[self.pathIndex]:
                        self.addNodeDict(statement_type)
                continue

            # Statement and argument end
            elif is_field_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.STATEMENT)
                self.lineIndex += 1
                continue

            elif is_command_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.lineIndex += 1
                continue

            # End of statements
            elif is_argument_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT)
                self.lineIndex += 1
                continue

            # Start word
            wordType = 1 if char in [' ', '\r', '\n', '\t'] else 2 if small or big or digit or is_special else 3
            if self.wordType != wordType:
                if self.currentWord is not None:
                    self.addNodeWord()
                self.wordType = wordType

            # Collect words
            self.currentWord = char if self.currentWord is None else self.currentWord + char
            self.lineIndex += 1

        assert self.pathIndex == len(self.pathList) - 1
        while len(self.pathList) > 1:
            if self.currentWord is not None:
                self.addNodeWord()
            if self.nodeList[self.pathIndex]:
                self.addNodeDict(NodeType.ENDSTATEMENT)
            assert len(self.pathList) > 1
            self.pathIndex = len(self.pathList) - 2
            self.pathList = self.pathList[0:-1]
            self.nodeList = self.nodeList[0:-1]

        assert self.pathIndex == len(self.pathList) - 1
        assert len(self.pathList) == 1
        if self.currentWord is not None:
            self.addNodeWord()
        if self.nodeList[self.pathIndex]:
            self.addNodeDict(NodeType.ENDSTATEMENT)
        assert len(self.pathList) == 1
        assert len(self.nodeList) == 1
        assert len(self.nodeList[0]) == 0
        self.scene = self.pathList[0]

    def addNodeDict(self, nodeType):
        assert self.pathIndex == len(self.pathList) - 1
        assert self.currentWord is None
        assert nodeType is not None
        assert self.nodeList[self.pathIndex]
        startIndex = self.nodeList[self.pathIndex][0].startIndex
        if self.nodeList[self.pathIndex][len(self.nodeList[self.pathIndex]) - 1].endIndex is None:
            self.nodeList[self.pathIndex][len(self.nodeList[self.pathIndex]) - 1].endIndex = self.lineIndex
        new_child = TreeNode(
            parentNode=self.pathList[self.pathIndex],
            nodeType=nodeType,
            textData=None,
            children=self.nodeList[self.pathIndex],
            startIndex=startIndex,
            endIndex=self.lineIndex,
            blockIndent=self.pathList[self.pathIndex].blockIndent
        )
        for item in new_child.childList:
            item.parentNode = item
        self.pathList[self.pathIndex].childList.append(new_child)
        self.nodeList[self.pathIndex] = []

    def addNodeWord(self):
        assert self.pathIndex == len(self.pathList) - 1
        assert self.currentWord is not None

        if self.wordType == 2 or self.wordType == 3:
            self.nodeList[self.pathIndex].append(TreeNode(
                parentNode=None,
                nodeType=NodeType.OPERATOR if self.currentWord in [
                    "const", "if", "for", "while", "type", "class", "string", "number", "boolean"] else NodeType.WORD if self.wordType == 2 else NodeType.OPERATOR,
                textData=self.currentWord,
                children=None,
                startIndex=self.lineIndex-len(self.currentWord),
                endIndex=self.lineIndex,
                blockIndent=self.pathList[self.pathIndex].blockIndent
            ))

        self.wordType = 0
        self.currentWord = None


class CppParser:
    textData: str
    lineIndex: int
    pathIndex: int
    pathList: list[TreeNode]
    nodeList: list[list[TreeNode]]
    wordType: int
    currentWord: str

    def __init__(self):
        self.textData = None
        self.lineIndex = 0
        self.pathIndex = 0
        self.pathList = None
        self.nodeList = None
        self.wordType = 0
        self.currentWord = None
        self.scene = None

    def parseTree(self, data):
        self.textData = data
        self.lineIndex = 0
        self.pathIndex = 0
        self.pathList = [TreeNode(
            parentNode=None,
            nodeType=NodeType.ROOT,
            textData=None,
            children=None,
            startIndex=0,
            endIndex=len(self.textData),
            blockIndent=0
        )]
        self.nodeList = [[]]
        self.wordType = 0
        self.currentWord = None
        self.is_done = False

        while self.lineIndex < len(self.textData) and not self.is_done:
            char = self.textData[self.lineIndex]
            two_char = self.textData[self.lineIndex:self.lineIndex+2]
            small = ord(char) >= ord('a') and ord(char) <= ord('z')
            big = ord(char) >= ord('A') and ord(char) <= ord('Z')
            digit = ord(char) >= ord('0') and ord(char) <= ord('9')
            is_special = char in ['.', '_', '#', ':']
            in_scope2 = self.pathList[self.pathIndex].nodeType == NodeType.WIGGLY or self.pathList[
                self.pathIndex].nodeType == NodeType.CURLY or self.pathList[self.pathIndex].nodeType == NodeType.SQUARE
            in_scope = self.pathList[
                self.pathIndex].nodeType == NodeType.CURLY or self.pathList[self.pathIndex].nodeType == NodeType.SQUARE
            is_command_end = char == ';'
            is_argument_end = in_scope2 and char == ','
            start_type = \
                NodeType.WIGGLY if char == '{' else \
                NodeType.CURLY if char == '(' else \
                NodeType.SQUARE if char == '[' else \
                None
            scope_end_char = \
                '}' if self.pathList[self.pathIndex].nodeType == NodeType.WIGGLY else \
                ')' if self.pathList[self.pathIndex].nodeType == NodeType.CURLY else \
                ']' if self.pathList[self.pathIndex].nodeType == NodeType.SQUARE else \
                None
            end_type = self.pathList[self.pathIndex].nodeType if scope_end_char is not None and char == scope_end_char else None
            is_include_end = \
                len(self.nodeList[self.pathIndex]) > 0 and \
                self.nodeList[self.pathIndex][0].nodeType == NodeType.WORD and \
                self.nodeList[self.pathIndex][0].textData == '#include' and \
                char == '\n'
            is_include_end |= \
                len(self.nodeList[self.pathIndex]) > 0 and \
                self.nodeList[self.pathIndex][0].nodeType == NodeType.WORD and \
                self.nodeList[self.pathIndex][0].textData == '#pragma' and \
                char == '\n'
            is_field_end = \
                len(self.nodeList[self.pathIndex]) >= 2 and \
                self.nodeList[self.pathIndex][1].getText() == ":" and \
                char == '\n'

            # Include end
            if is_include_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.lineIndex += 1
                continue

            # Strings
            if char in ['"', "'", "`"]:
                if self.currentWord is not None:
                    self.addNodeWord()
                end_chr = char
                endIndex = self.textData.find(end_chr, self.lineIndex + 1)
                assert endIndex != -1
                value = self.textData[self.lineIndex+1:endIndex]
                self.nodeList[self.pathIndex].append(TreeNode(
                    parentNode=self.pathList[self.pathIndex],
                    nodeType=NodeType.STRING,
                    textData=value,
                    children=None,
                    startIndex=self.lineIndex,
                    endIndex=endIndex,
                    blockIndent=self.pathList[self.pathIndex].blockIndent
                ))
                self.lineIndex = endIndex + 1
                continue

            # Comments
            if two_char in ['//', '/*']:
                if self.currentWord is not None:
                    self.addNodeWord()
                endIndex = self.textData.find('\n' if two_char == '//' else '*/', self.lineIndex)
                if endIndex == -1:
                    self.is_done = True
                    continue
                comment = self.textData[self.lineIndex:endIndex]
                if len(self.nodeList[self.pathIndex]) > 0:
                    self.nodeList[self.pathIndex].append(TreeNode(
                        parentNode=self.pathList[self.pathIndex],
                        nodeType=NodeType.COMMENT,
                        textData=comment,
                        children=None,
                        startIndex=self.lineIndex,
                        endIndex=endIndex,
                        blockIndent=self.pathList[self.pathIndex].blockIndent,
                    ))
                else:
                    self.pathList[self.pathIndex].childList.append(TreeNode(
                        parentNode=self.pathList[self.pathIndex],
                        nodeType=NodeType.COMMENT,
                        textData=comment,
                        children=None,
                        startIndex=self.lineIndex,
                        endIndex=endIndex,
                        blockIndent=self.pathList[self.pathIndex].blockIndent,
                    ))
                self.lineIndex = endIndex + 2
                continue

            # Scope starts and ends
            if start_type is not None:
                if self.currentWord is not None:
                    self.addNodeWord()
                new_child = TreeNode(
                    parentNode=self.pathList[self.pathIndex],
                    nodeType=start_type,
                    textData=None,
                    children=None,
                    startIndex=self.lineIndex,
                    endIndex=None,
                    blockIndent=0,
                )
                self.nodeList[self.pathIndex].append(new_child)
                self.pathIndex = len(self.pathList)
                self.pathList = [*self.pathList, new_child]
                self.nodeList = [*self.nodeList, []]
                self.lineIndex += 2 if start_type == NodeType.BLOCK else 1
                continue

            elif end_type is not None:
                assert len(self.pathList) > 1
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.pathList[self.pathIndex].endIndex = self.lineIndex
                self.pathIndex = len(self.pathList) - 2
                self.pathList = self.pathList[0:-1]
                self.nodeList = self.nodeList[0:-1]
                self.lineIndex += 1
                # includes are 4-item long statements
                if end_type == NodeType.WIGGLY and len(self.nodeList[self.pathIndex]) == 2 and \
                        self.nodeList[self.pathIndex][0].getText() == "import":
                    pass
                # class/function-body/dictionary-definition should be a statement
                elif end_type == NodeType.WIGGLY:
                    statement_type = \
                        NodeType.STATEMENT if end_type == NodeType.WIGGLY else \
                        NodeType.CALL if end_type == NodeType.CURLY else \
                        None
                    if self.currentWord is not None:
                        self.addNodeWord()
                    if self.nodeList[self.pathIndex]:
                        self.addNodeDict(statement_type)
                # class annotation should be a statement
                elif end_type == NodeType.CURLY and len(self.nodeList[self.pathIndex]) > 0 and self.nodeList[self.pathIndex][0].getText() == "@":
                    statement_type = NodeType.STATEMENT
                    if self.currentWord is not None:
                        self.addNodeWord()
                    if self.nodeList[self.pathIndex]:
                        self.addNodeDict(statement_type)
                continue

            # Statement and argument end
            elif is_field_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.STATEMENT)
                self.lineIndex += 1
                continue

            elif is_command_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT if in_scope else NodeType.STATEMENT)
                self.lineIndex += 1
                continue

            # End of statements
            elif is_argument_end:
                if self.currentWord is not None:
                    self.addNodeWord()
                if self.nodeList[self.pathIndex]:
                    self.addNodeDict(NodeType.ARGUMENT)
                self.lineIndex += 1
                continue

            # Start word
            wordType = 1 if char in [' ', '\r', '\n', '\t'] else 2 if small or big or digit or is_special else 3
            if self.wordType != wordType:
                if self.currentWord is not None:
                    self.addNodeWord()
                self.wordType = wordType

            # Collect words
            self.currentWord = char if self.currentWord is None else self.currentWord + char
            self.lineIndex += 1

        assert self.pathIndex == len(self.pathList) - 1
        while len(self.pathList) > 1:
            if self.currentWord is not None:
                self.addNodeWord()
            if self.nodeList[self.pathIndex]:
                self.addNodeDict(NodeType.ENDSTATEMENT)
            assert len(self.pathList) > 1
            self.pathIndex = len(self.pathList) - 2
            self.pathList = self.pathList[0:-1]
            self.nodeList = self.nodeList[0:-1]

        assert self.pathIndex == len(self.pathList) - 1
        assert len(self.pathList) == 1
        if self.currentWord is not None:
            self.addNodeWord()
        if self.nodeList[self.pathIndex]:
            self.addNodeDict(NodeType.ENDSTATEMENT)
        assert len(self.pathList) == 1
        assert len(self.nodeList) == 1
        assert len(self.nodeList[0]) == 0
        self.scene = self.pathList[0]

    def addNodeDict(self, nodeType):
        assert self.pathIndex == len(self.pathList) - 1
        assert self.currentWord is None
        assert nodeType is not None
        assert self.nodeList[self.pathIndex]
        startIndex = self.nodeList[self.pathIndex][0].startIndex
        if self.nodeList[self.pathIndex][len(self.nodeList[self.pathIndex]) - 1].endIndex is None:
            self.nodeList[self.pathIndex][len(self.nodeList[self.pathIndex]) - 1].endIndex = self.lineIndex
        new_child = TreeNode(
            parentNode=self.pathList[self.pathIndex],
            nodeType=nodeType,
            textData=None,
            children=self.nodeList[self.pathIndex],
            startIndex=startIndex,
            endIndex=self.lineIndex,
            blockIndent=self.pathList[self.pathIndex].blockIndent
        )
        for item in new_child.childList:
            item.parentNode = item
        self.pathList[self.pathIndex].childList.append(new_child)
        self.nodeList[self.pathIndex] = []

    def addNodeWord(self):
        assert self.pathIndex == len(self.pathList) - 1
        assert self.currentWord is not None

        if self.wordType == 2 or self.wordType == 3:
            self.nodeList[self.pathIndex].append(TreeNode(
                parentNode=None,
                nodeType=NodeType.OPERATOR if self.currentWord in [
                    "public:", "private:", "protected:", "const", "if", "for", "while", "type", "class", "string", "number", "boolean"] else NodeType.WORD if self.wordType == 2 else NodeType.OPERATOR,
                textData=self.currentWord,
                children=None,
                startIndex=self.lineIndex-len(self.currentWord),
                endIndex=self.lineIndex,
                blockIndent=self.pathList[self.pathIndex].blockIndent
            ))

        self.wordType = 0
        self.currentWord = None


def parseSchemaScene(file):
    result = []
    data = open(file, 'rt').read()
    isPy = file.endswith('.py')
    isTs = file.endswith('.ts')
    isCpp = file.endswith('.h') or file.endswith('.hpp')
    p = TypescriptParser() if isTs else PythonParser() if isPy else CppParser()
    p.parseTree(data)
    scene = p.scene
    typeMap = {
        "std::string": "str",
        "string": "str",
        "nlohmann::json": "dict",
        "json": "dict",
        "boolean": "bool",
        "object": "dict",
        "Object": "dict",
        "any": "dict",
        "list": "list",
        "Array": "list",
        "unknown": "dict",
    }

    def getType(text):
        return typeMap.get(text, text)

    for item in scene.children:
        matched1 = (
            item.getParams(["statement", "class", "*", "wiggly"]) or
            item.getParams(["statement", "export", "class", "*", "wiggly"]) or
            item.getParams(["statement", "export", "abstract", "class", "*", "wiggly"])
        )

        if matched1:
            assert len(matched1) == 1
            class_name = matched1[0]
            for item2 in item.children[len(item.children) - 1].children:
                matched1 = (
                    item2.getParams(["statement", "public:", "*", "*"]) or
                    item2.getParams(["statement", "*", "*"])
                )
                if isCpp and matched1:
                    result.append({
                        "className": class_name,
                        "fieldName": matched1[1],
                        "fieldType": getType(matched1[0]),
                        "idNumber": -1
                    })
                matched1b = (
                    item2.getParams(["statement", "public:", "virtual", "*", "*", "curly", "=", "0"]) or
                    item2.getParams(["statement", "virtual", "*", "*", "curly", "=", "0"])
                )
                matched1b2 = (
                    None if not matched1b else
                    item2.childList[3].childList[0].getParams(['argument', '*', '*']) if item2.childList[3].nodeType == NodeType.CURLY else
                    item2.childList[4].childList[0].getParams(['argument', '*', '*']) if item2.childList[4].nodeType == NodeType.CURLY else
                    None
                )
                if isCpp and matched1b and matched1b2:
                    result.append({
                        "className": class_name,
                        "methodName": matched1b[1],
                        "methodRequest": getType(matched1b2[0]),
                        "methodResponse": getType(matched1b[0]),
                        "idNumber": -1
                    })
                matched2 = (
                    item2.getParams(["statement", "*", ":", "*"])
                )
                if matched2:
                    result.append({
                        "className": class_name,
                        "fieldName": matched2[0],
                        "fieldType": getType(matched2[1]),
                        "idNumber": -1
                    })
                matched3 = item2.getParams(["statement", "abstract", "*", "curly", ":", "Promise", "<", "*", ">"])
                matched4 = item2.childList[2].children[0].getParams(
                    ["argument", "*", ":", "*"]) if matched3 else None
                if matched3 and matched4:
                    result.append({
                        "className": class_name,
                        "methodName": matched3[0],
                        "methodRequest": getType(matched4[1]),
                        "methodResponse": getType(matched3[1]),
                        "idNumber": -1
                    })

                matched5 = item2.getParams(['statement', 'def', '*', 'curly', '->', '*', ':', '...'])
                if matched5 and item2.childList[2].childList[0].getParams(["argument", "*"]) != ["self"]:
                    raise RuntimeError(f"Missing self argument in: {class_name}.{matched5[0]}")
                matched6 = item2.childList[2].childList[1].getParams(
                    ['argument', '*', ':', '*']) if matched5 else None
                if matched5 and not matched6:
                    raise RuntimeError(f"Missing two arguments and a type: {matched5}")
                if matched5 and matched6:
                    result.append({
                        "className": class_name,
                        "methodName": matched5[0],
                        "methodRequest": getType(matched6[1]),
                        "methodResponse": getType(matched5[1]),
                        "idNumber": -1
                    })

    return result, scene


def parseSchemaList(file):
    result, _ = parseSchemaScene(file)
    return result


if __name__ == "__main__":
    file = sys.argv[1]
    assert file
    assert os.path.exists(file)
    result, scene = parseSchemaScene(file)
    print('\n'.join(scene.getFormatted('    ')), file=sys.stderr, flush=True)
    print(json.dumps(result, indent=4), file=sys.stdout, flush=True)
