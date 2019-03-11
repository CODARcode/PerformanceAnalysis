from mpi4py import MPI
import adios2
import numpy as np

from enum import Enum

class OpenMode(Enum):
    READ   = "r"
    WRITE  = "w"
    APPEND = "a" # not yet supported

class EngineType(Enum):
    BP3     = "BP"
    HDF5    = "HDF5"
    DataMan = "DataMan"
    SST     = "SST"

    @staticmethod
    def get_type(t:str):
        t = t.lower()
        if 'bp' in t:
            return EngineType.BP3
        elif 'sst' in t:
            return EngineType.SST
        else:
            raise Exception("Unsupported engine type: %s" % t)

def get_npdtype(t:str):
    t = t.lower()
    if   t == "char":                   return np.dtype(np.int8)
    elif t == "signed char":            return np.dtype(np.int8)
    elif t == "unsigned char":          return np.dtype(np.uint8)
    elif t == "short":                  return np.dtype(np.int16)
    elif t == "unsinged short":         return np.dtype(np.uint16)
    elif t == "int":                    return np.dtype(np.int32)
    elif t == "unsigned int":           return np.dtype(np.uint32)
    elif t == "long int":               return np.dtype(np.int64)
    elif t == "long long int":          return np.dtype(np.int64)
    elif t == "unsigned long int":      return np.dtype(np.uint64)
    elif t == "unsigned long long int": return np.dtype(np.uint64)
    elif t == "float":                  return np.dtype(np.float32)
    elif t == "double":                 return np.dtype(np.float64)
    elif t == "long double":            return np.dtype(np.float128)
    elif t == "string":                 return np.dtype(np.str)
    else:
        raise ValueError("Unknown data type: %s" % t)

class pyVariable(object):
    def __init__(
            self, name,
            SingleValue = 'true', # 'true' or 'false'
            Type = '',
            Value = '',
            Shape = '',
            AvailableStepsCount = '',
            Min = '', Max = ''
    ):
        self.name = name
        self.is_scalar = True if SingleValue == 'true' else False
        self.n_steps = int(AvailableStepsCount)
        self.shape = () if len(Shape) == 0 else [int(t) for t in Shape.split(',')]
        self.dtype = None if len(Type) == 0 else get_npdtype(Type)
        self.type = Type
        # Note that in adios2.x Value string comes with `"str"'.
        if Type == 'string':
            Value = Value.replace('"', '')
        self.value = np.array([Value], dtype=self.dtype) if self.is_scalar else None
        self.min = Min # string
        self.max = Max # string

    def __repr__(self):
        return str(dict(
            name=self.name,
            is_scalar=self.is_scalar,
            n_steps=self.n_steps,
            shape=self.shape,
            dtype=self.dtype,
            type=self.type,
            value=self.value,
            min=self.min,
            max=self.max
        ))

class pyAttribute(object):
    def __init__(self, name, Elements:str='', Value:str='', Type:str=''):
        self.name = name
        self.n_elements = int(Elements)
        self.dtype = get_npdtype(Type)
        self.type = Type
        # Note that in adios2.x Value string comes with `"str"'.
        if Type == 'string':
            Value = Value.replace('"', '')
        self._value = np.array([Value], dtype=self.dtype)

    def __repr__(self):
        return str(dict(
            name=self.name,
            n_elements=self.n_elements,
            dtype=self.dtype,
            type=self.type,
            value=self._value
        ))

    def value(self):
        if self.type == 'string': return self._value[0]
        return self._value.copy()



class pyAdios(object):
    """ pyAdios class

    Wrapper class for Adios 2.x

    """

    def __init__(self, method:str, parameters:str=None):
        # MPI
        # todo: what would be the correct way to handle MPI?
        self.comm = MPI.COMM_WORLD
        self.rank = self.comm.Get_rank()
        self.size = self.comm.Get_size()

        # adios engine type
        self.engine_type = EngineType.get_type(method)
        # adios engine parameter
        self.parameters = dict()
        if parameters is not None:
            for param in parameters.split():
                key, value = param.split("=")
                self.parameters[key] = value

        # adios file handler
        self.fh = None
        # adios file stream (only available for reader)
        self.stream = None

    def open(self, filename:str, mode:OpenMode=OpenMode.READ):
        """
        Open a file with adios2

        Args:
            filename: (str), filename with full path
            mode: (OpenMode), open mode

        Returns:

        """

        self.fh = adios2.open(filename, mode.value, self.comm, self.engine_type.value)
        self.fh.set_parameters(self.parameters)
        if mode == OpenMode.READ:
            self.stream = self.fh.__next__()

    def current_step(self):
        """
        With invalid stream, it returns -1.
        Returns: (int), current step in the stream
        """
        if self.stream is None: return -1
        return self.stream.current_step()

    def advance(self):
        """
        Advance to next step

        Returns:
            return current_step if next stream is available; otherwise -1
        """
        status = -1
        try:
            # note: in streaming mode releases the current_step
            # note: no effect in file based engines
            self.fh.end_step()

            self.stream = self.fh.__next__()
            status = self.current_step()
        except StopIteration:
            pass
        return status

    def available_variables(self):
        """
        Typical way to dump the available variables...

            vars = self.stream.available_variables()
            for name, info in vars.items():
                print("variable_name: " + name)
                for key, value in info.items():
                    print("\t" + key + ": " + value)
                print("\n")

        With invalid stream, it returns empty dictionary.
        Returns: (dict), available variables in the current stream
        """
        if self.stream is None: return dict()
        vars = dict()
        for name, info in self.stream.available_variables().items():
            vars[name] = pyVariable(name, **info)
        return vars

    def available_attributes(self):
        """
        Typical way to dump the available attributes...

            vars = self.stream.available_attributes()
            for name, info in vars.items():
                print("attribute_name: " + name)
                for key, value in info.items():
                    print("\t" + key + ": " + value)
                print("\n")

        With invalid stream, it returns empty dictionary.
        Returns: (dict), available variables in the current stream
        """
        if self.stream is None: return dict()
        atts = dict()
        for name, info in self.stream.available_attributes().items():
            atts[name] = pyAttribute(name, **info)
        return atts

    def read_variable(self, var_name:str,
                          start:list=None, count:list=None,
                          step_start:int=None, step_count:int=None):
        """
        Read a variable

        Args:
            var_name: str, variable name
            start: [int], start position (only for array data)
            count: [int], number of elements (only for array data)
            step_start: int, start step (only for array data)
            step_count: int, number of steps (only for array data)

        Returns:
            stored data of `var_name`
        """
        variables = self.available_variables()
        if var_name not in variables.keys(): return None

        v = variables[var_name]
        if v.is_scalar:
            return self.stream.read(var_name)

        # todo: validate step start and step count
        if step_start is None: step_start = self.current_step()
        else:                  step_start = min(self.current_step(), step_start)
        if step_count is None: step_count = 1
        else:                  step_count = min(step_count, v.n_steps - step_start)

        ndim = len(v.shape)
        if start is None: start = [0] * ndim
        if count is None: count = v.shape

        return self.stream.read(var_name, start, count)
        #return self.stream.read(var_name, start, count, step_start, step_count)

    def write_variable(self, var_name:str, data:np.ndarray,
                       start:list=None, count:list=None, end_step:bool=False):
        """
        Write a variable

        Args:
            var_name: str, variable name
            data: np.ndarray, data
            start: [int], start position (only for array data)
            count: [int], number of elements (only for array data)
            end_step: bool, if True, call end_step() after writing this variable.
        """
        if start is not None and count is not None:
            self.fh.write(var_name, data, data.shape, start, count, end_step)
        else:
            self.fh.write(var_name, data, end_step)

    def write_attribute(self, att_name:str, data):
        """
        Write a attribute
        Args:
            att_name: str, attribute name
            data: [np.ndarray, str], attribute value either np.ndarray (for number) or string
        """
        self.fh.write_attribute(att_name, data)

    def end_step(self):
        self.fh.end_step()

    def close(self):
        self.stream = None
        if self.fh is not None:
            self.fh.close()
        self.fh = None

























