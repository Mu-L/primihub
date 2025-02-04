from primihub.utils.logger_util import logger


class PreprocessBase:

    def __init__(self, FL_type=None, role=None, channel=None):
        if FL_type in ['V', 'H']:
            self.FL_type = FL_type
        else:
            error_msg = f"Unsupported FL type: {FL_type}"
            logger.error(error_msg)
            raise RuntimeError(error_msg)

        if role in ['host', 'guest', 'client', 'server']:
            self.role = role
        else:
            error_msg = f"Unsupported role: {role}"
            logger.error(error_msg)
            raise RuntimeError(error_msg)
        
        self.channel = channel

    def Vfit(self, x):
        return self.module.fit(x)

    def Hfit(self, x):
        pass

    def fit(self, x):
        if self.FL_type == 'V':
            return self.Vfit(x)
        else:
            return self.Hfit(x)

    def fit_transform(self, x):
        self.fit(x)
        return self.module.transform(x)
        