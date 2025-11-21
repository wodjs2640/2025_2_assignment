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

# def YourLRScheduler(param):
#     def lr_lambda:
#         raise NotImplementedError
#     return lr_lambda

scheduler_func_dict = {
    "MultiStepLR": MultiStepLR,
    # "YourLRScheduler: : YourLRScheduler
}

def get_scheduler(name, param, optimizer):
    lr_lambda = scheduler_func_dict[name](param)
    scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer, lr_lambda)
    return scheduler
