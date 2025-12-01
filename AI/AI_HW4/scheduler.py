import torch

def MultiStepLR(param):
    milestones = param.get("milestones", [])
    gamma = param.get("gamma", [])

    milestones = sorted(milestones)

    def lr_lambda(epoch):
        count = 0
        for m in milestones:
            if epoch >= m:
                count += 1
        return gamma ** count

    return lr_lambda

def ExponentialDecay(param):
    gamma = param.get("gamma", 0.95)

    def lr_lambda(epoch):
        return gamma ** epoch

    return lr_lambda

def CosineAnnealingLR(param):
    T_max = param.get("T_max", 50)
    eta_min_ratio = param.get("eta_min_ratio", 0.0)

    def lr_lambda(epoch):
        import math
        return eta_min_ratio + (1 - eta_min_ratio) * (1 + math.cos(math.pi * epoch / T_max)) / 2

    return lr_lambda

def WarmupCosineDecay(param):
    warmup_epochs = param.get("warmup_epochs", 5)
    T_max = param.get("T_max", 50)
    eta_min_ratio = param.get("eta_min_ratio", 0.0)

    def lr_lambda(epoch):
        import math
        if epoch < warmup_epochs:
            return (epoch + 1) / warmup_epochs
        else:
            progress = (epoch - warmup_epochs) / (T_max - warmup_epochs)
            return eta_min_ratio + (1 - eta_min_ratio) * (1 + math.cos(math.pi * progress)) / 2

    return lr_lambda

scheduler_func_dict = {
    "MultiStepLR": MultiStepLR,
    "ExponentialDecay": ExponentialDecay,
    "CosineAnnealingLR": CosineAnnealingLR,
    "WarmupCosineDecay": WarmupCosineDecay,
}

def get_scheduler(name, param, optimizer):
    lr_lambda = scheduler_func_dict[name](param)
    scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer, lr_lambda)
    return scheduler
